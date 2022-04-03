# A script to copy positions using Interactive Brokers because SierrChart does
# not support adaptive market orders

import asyncio
import json
import logging
import typing as t
import datetime as dt
from asyncio.tasks import ensure_future
from contextlib import suppress

import click
from ib_insync import ContFuture, Contract, IB, MarketOrder, TagValue, Order, Trade, Forex, BarDataList, Stock, Future, Option, LimitOrder, Order
from importlib.machinery import SourceFileLoader


class IBConfig(t.TypedDict):
    host: str
    port: int
    clientId: int


class TradeSpecConfig(t.TypedDict):
    contract: Contract
    multiplier: float
    port: int


class Config(t.TypedDict):
    ib: IBConfig
    chartbookHost: str
    tradeSpecs: list[TradeSpecConfig]


def getIBPosition(ib: IB, contract: Contract) -> float:
    ibPositions = ib.positions()
    ibPositionQty: float = 0
    for ibPosition in ibPositions:
        if ibPosition.contract.conId == contract.conId:
            return ibPosition.position
    return 0


async def reader(ib: IB, multiplier: float, chartbook_host: str,
                 contract: Contract, chartbook_port: int) -> None:

    targetPosition = 0
    symbol = contract.symbol
    if isinstance(contract, Forex):
        symbol += contract.currency
    logger = logging.getLogger(f"{chartbook_host} {chartbook_port} {ib.client.port} {symbol}")
    trade: t.Optional[Trade] = None

    while True:
        try:
            logger.info(f"Connecting to {chartbook_host}:{chartbook_port}")
            reader, writer = await asyncio.open_connection(host=chartbook_host,
                                                           port=chartbook_port)

            while True:
                line = await asyncio.wait_for(reader.readline(), timeout=5)
                jsonObj = json.loads(line)
                logger.info(jsonObj)
                isNewTargetPosition = False
                if 'position' in jsonObj:
                    isNewTargetPosition = True
                    targetPosition = jsonObj['position']

                ibPosition = getIBPosition(ib, contract)

                if trade is not None:
                    if not trade.isActive():  # type: ignore
                        trade = None

                delta = multiplier * targetPosition - ibPosition
                if isNewTargetPosition:
                    logger.info(f"{targetPosition=}, {ibPosition=}, {delta=}")
                if delta != 0:
                    action = "BUY" if delta > 0 else "SELL"

                    if trade is not None and trade.isActive():  # type: ignore
                        cancelOrder = trade.order.action != action
                        # if we've got an active trade, and the remaining quantity
                        # does not match our desired delta, then we cancel the
                        # order and create a new trade
                        remaining = abs(trade.remaining())  # type: ignore
                        cancelOrder |= remaining != abs(delta)
                        if cancelOrder:
                            logger.info(f"Cancelling order: {trade.order}")
                            ib.cancelOrder(trade.order)
                            trade = None

                    # If we have no trade, or we just cancelled one
                    if trade is None:
                        order: Order
                        if contract.secType in ['FUT', 'STK', 'OPT','CONTFUT']:
                            order = MarketOrder(action,
                                                abs(delta),
                                                algoStrategy='Adaptive',
                                                algoParams=[
                                                    TagValue(
                                                        'adaptivePriority',
                                                        'Urgent')
                                                ])
                        else:
                            assert contract.secType == 'CASH'
                            # TODO: use midpoint or cross spread specifically
                            order = MarketOrder(
                                action,
                                abs(delta))
                        logger.info(f"Placing order: {order}")
                        trade = ib.placeOrder(contract, order)
                        trade.statusEvent += lambda t: logger.info(str(t))

        except asyncio.TimeoutError:
            logger.info("Timed out, trying to connect again")
        except Exception:
            logger.exception("Unexpected exception")
        await asyncio.sleep(5)


async def ensureIbConnected(ib: IB, ib_host: str, ib_port: int,
                            ib_client_id: int) -> None:
    probeContract = Forex("EURUSD")
    probeTimeout = dt.timedelta(seconds=4)
    connectTimeout = dt.timedelta(seconds=4)
    idleTimeout = dt.timedelta(seconds=30)

    logger = logging.getLogger("ensureIbConnected")
    waiter: t.Optional[asyncio.Future[None]] = None
    while True:
        try:

            def onTimeout(_: t.Any) -> None:
                logger.warning(f"onTimeout")
                if waiter and not waiter.done():
                    waiter.set_result(None)

            def onError(reqId: int, errorCode: int, errorString: str,
                        contract: Contract) -> None:
                logger.warning(
                    f"onError({reqId=},{errorCode=},{errorString=},{contract=})"
                )
                if waiter and errorCode in {100, 1100, 1102
                                            } and not waiter.done():
                    waiter.set_exception(Warning(f'Error {errorCode}'))

            def onDisconnected() -> None:
                logger.warning("onDisconnected")
                if waiter and not waiter.done():
                    waiter.set_exception(Warning("Disconnected"))

            ib.setTimeout(idleTimeout.total_seconds())
            ib.timeoutEvent += onTimeout
            ib.errorEvent += onError
            ib.disconnectedEvent += onDisconnected

            logger.info(
                f"Connecting to IB: {ib_host}:{ib_port}#{ib_client_id}")

            ib.disconnect()  # type: ignore
            await ib.connectAsync(host=ib_host,
                                  port=ib_port,
                                  clientId=ib_client_id,
                                  timeout=connectTimeout.total_seconds())

            logger.info("Connected")
            await asyncio.sleep(0.25)

            while True:
                waiter = asyncio.Future()

                # This will only be done if we get a timeout (normal return) or
                # an error/disconnection => exception
                await waiter
                logger.info("Soft timeout occurred, probing for data")

                task = ib.reqHistoricalDataAsync(probeContract, '', '30 S',
                                                 '5 secs', 'MIDPOINT', False)

                bars: t.Optional[BarDataList] = None
                with suppress(asyncio.TimeoutError):
                    bars = await asyncio.wait_for(task,
                                                  probeTimeout.total_seconds())
                if not bars:
                    raise Warning("Hard timeout")
                logger.info("Looks like we are still connected")

        except Warning as w:
            logger.warning(w)
        except Exception:
            logger.exception("Unexpected exception")
        finally:
            ib.disconnectedEvent -= onDisconnected
            ib.errorEvent -= onError
            ib.timeoutEvent -= onTimeout


async def start(config: Config) -> None:
    ib = IB()  # type: ignore

    futs = []

    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s.%(msecs)03d %(levelname)s %(name)s: %(message)s')
    logging.getLogger('ib_insync').setLevel(logging.WARNING)

    await ib.connectAsync(host=config['ib']['host'],
                          port=config['ib']['port'],
                          clientId=config['ib']['clientId'])

    for spec in config['tradeSpecs']:
        try:
            contract = spec['contract']
            multiplier = spec['multiplier']
            port: int = spec['port']

            ret = await ib.qualifyContractsAsync(contract)
            if ret:
                logging.info(f"Using contract: {contract}")
                task = reader(ib, multiplier, config['chartbookHost'],
                              contract, port)
                fut = ensure_future(task)
                futs.append(fut)
            else:
                logging.error(f"Contract not found for spec {spec}")
        except Exception:
            logging.exception(f"Error with {spec}")

    if len(futs):
        futs.append(
            ensure_future(
                ensureIbConnected(ib, config['ib']['host'],
                                  config['ib']['port'],
                                  config['ib']['clientId'])))

    await asyncio.wait(futs)

async def runConfigs(configs:list[Config]) -> None:
    futs = []
    for config in configs:
        futs.append(
            ensure_future(start(config))
        )
    await asyncio.wait(futs)

@click.command()
@click.argument("config_module_py", type=click.STRING)
def main(config_module_py: str) -> None:
    module = SourceFileLoader("config", config_module_py).load_module()
    configs: list[Config] = module.get()

    loop = asyncio.new_event_loop()
    task = loop.create_task(runConfigs(configs))
    loop.run_until_complete(task)


main()
