# Trading Engine

This repository is a compact C++ trading engine built around daily equity data. The idea is straightforward: read bars from CSV, turn recent market behavior into features, train simple models in a walk-forward loop, simulate trades with basic friction and risk controls, and report only out-of-sample results.

## What The Project Does

At a high level, the engine follows this loop:

1. read daily OHLCV data
2. build rolling features from past bars
3. train models on a historical window
4. test those models on the next unseen window
5. simulate fills, exits, fees, and portfolio rules
6. report portfolio performance

The default setup is:

- train window: `252` trading days
- test window: `63` trading days
- capital: `100000`

Signals are formed after day `t` is complete and are filled on day `t+1` open, so the backtest does not peek ahead.

## File Guide

### Root files

- [market_data.csv](C:/Users/harsh/Documents/Stock_Predictor/market_data.csv:1): sample input data used by the engine when passed with `--file`.
- [fetch_data.py](C:/Users/harsh/Documents/Stock_Predictor/fetch_data.py:1): Yahoo Finance downloader that writes a flat CSV the engine can read directly.
- [train_models.py](C:/Users/harsh/Documents/Stock_Predictor/train_models.py:1): offline Python training script that mirrors the C++ features, trains logistic and ridge models, and exports a generated C++ header.
- `model.json`: reusable model file written by the C++ `train` mode.
- [build.ps1](C:/Users/harsh/Documents/Stock_Predictor/build.ps1:1): PowerShell build shortcut for Windows.
- [test.ps1](C:/Users/harsh/Documents/Stock_Predictor/test.ps1:1): PowerShell script that compiles and runs the unit tests.
- [Makefile](C:/Users/harsh/Documents/Stock_Predictor/Makefile:1): optional build/test helper for setups that use `make`.
- [README.md](C:/Users/harsh/Documents/Stock_Predictor/README.md:1): project overview, structure notes, and run instructions.

### `include/`

- [include/Event.h](C:/Users/harsh/Documents/Stock_Predictor/include/Event.h:1): shared enums and structs such as `Bar`, `Signal`, `Order`, `Fill`, `Trade`, `Side`, and exit reasons.
- [include/Datahandler.h](C:/Users/harsh/Documents/Stock_Predictor/include/Datahandler.h:1): interface for loading CSV data into bars.
- [include/Features.h](C:/Users/harsh/Documents/Stock_Predictor/include/Features.h:1): feature-row definition and feature-engine interface.
- [include/Models.h](C:/Users/harsh/Documents/Stock_Predictor/include/Models.h:1): standardizer, logistic model, ridge model, and training helpers.
- [include/GeneratedModels.h](C:/Users/harsh/Documents/Stock_Predictor/include/GeneratedModels.h:1): generated coefficients exported by `train_models.py`.
- [include/Strategy.h](C:/Users/harsh/Documents/Stock_Predictor/include/Strategy.h:1): interface for turning model output into `Buy`, `Sell`, or `Hold`.
- [include/Execution.h](C:/Users/harsh/Documents/Stock_Predictor/include/Execution.h:1): interface for simulating slippage and commission.
- [include/Portfolio.h](C:/Users/harsh/Documents/Stock_Predictor/include/Portfolio.h:1): portfolio state, positions, equity points, and risk configuration.
- [include/Performance.h](C:/Users/harsh/Documents/Stock_Predictor/include/Performance.h:1): performance report structure and analyzer interface.
- [include/Backtest.h](C:/Users/harsh/Documents/Stock_Predictor/include/Backtest.h:1): walk-forward backtest configuration and result types.

### `src/`

- [src/main.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/main.cpp:1): command-line entry point. It parses flags, loads data, runs the backtest, and prints the report.
- [src/DataHandler.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/DataHandler.cpp:1): CSV loader. It supports Yahoo-style files and flat `Date,Symbol,Open,High,Low,Close,Volume` data.
- [src/Features.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Features.cpp:1): feature builder. It computes returns, rolling volatility, ATR, volume z-score, and FFT band energy.
- [src/Models.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Models.cpp:1): model training and inference helpers. It fits logistic and ridge models in C++ and can load generated weights.
- [src/Strategy.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Strategy.cpp:1): converts model output into trading signals.
- [src/Execution.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Execution.cpp:1): turns a raw market price into a more realistic fill by adding slippage and commission.
- [src/Portfolio.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Portfolio.cpp:1): handles position sizing, open positions, cash, stops, targets, drawdown, and trade history.
- [src/Performance.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Performance.cpp:1): computes CAGR, Sharpe, Sortino, max drawdown, win rate, and related summary metrics.
- [src/Backtest.cpp](C:/Users/harsh/Documents/Stock_Predictor/src/Backtest.cpp:1): runs walk-forward validation, retrains per fold, routes fills, and keeps evaluation out of sample.

### `tests/`

- [tests/test_main.cpp](C:/Users/harsh/Documents/Stock_Predictor/tests/test_main.cpp:1): lightweight unit tests for strategy decisions, portfolio bookkeeping, and performance math.

## What `test` Is Doing

The test layer is there to catch broken logic, not to prove that the strategy is profitable.

Current checks:

- `Strategy`: strong positive input should become `Buy`, strong negative input should become `Sell`, weak input should stay `Hold`
- `Portfolio`: positions should open and close correctly, and completed trades should be recorded
- `PerformanceAnalyzer`: report values should match a simple fake trade history

If `./test.ps1` passes, the code behavior is still consistent after edits. It does not mean the trading idea is automatically strong.

## What `train_models.py` Is Doing

`train_models.py` is the offline training helper. It:

1. reads CSV market data
2. computes the same 16 features used by the C++ engine
3. trains a logistic model for next-day direction
4. trains a ridge model for next-day return size
5. writes the learned coefficients to `include/GeneratedModels.h`

This is mainly useful as a saved model snapshot in header form.

Important detail:

- the walk-forward engine still retrains models inside each fold
- the generated header acts more like an exported snapshot and fallback source

## What `Execution` Is Doing

Yahoo Finance provides real market bars, but not the exact trade fill an account would get.

A daily bar can say:

- open = `100`
- high = `103`
- low = `99`
- close = `102`
- volume = `1000000`

That still does not answer:

- whether a buy really filled at exactly `100`
- how much slippage the order suffered
- what fee the trade paid

`Execution.cpp` fills that gap by:

- adding slippage based on order size relative to volume
- charging commission
- returning a realistic `Fill` object instead of assuming a perfect free trade

In short:

- the bar gives the market price reference
- the execution model estimates the actual trade price

## Feature Set

The engine uses 16 features built from recent price and volume behavior:

- last return
- rolling means of returns
- rolling volatility
- ATR
- intraday range
- volume z-score
- FFT spectral band features

The FFT side is grouped into energy bands instead of isolated bins:

- `low`: bins `1-2`
- `mid`: bins `3-5`
- `high`: bins `6-8`

Those bands are meant to summarize slower swings, medium cycles, and faster noisy movement.

## How The Backtest Runs

When the engine runs, the timeline looks like this:

1. load all bars from the CSV
2. split the date history into walk-forward folds
3. train on the first `train` block
4. test only on the next `test` block
5. roll forward and repeat

Inside a test fold:

1. use only information available up to day `t`
2. build a signal after day `t` is complete
3. fill that order on day `t+1` open
4. manage stop loss, take profit, and drawdown controls
5. mark end-of-day equity

That is why trading does not start from the first row. The engine needs enough history for features and then a full training window before out-of-sample trading begins.

## How To Run

### Build

```powershell
./build.ps1
```

That creates `quant_engine.exe`.

### Train

```powershell
./quant_engine.exe --mode train --file market_data.csv --model model.json
```

This reads the CSV, trains on the available feature rows, and saves the reusable model file.

### Predict

```powershell
./quant_engine.exe --mode predict --file market_data.csv --model model.json
```

This loads a saved model and prints the latest score for each symbol.

### Backtest

Walk-forward backtest:

```powershell
./quant_engine.exe --mode backtest --file market_data.csv --train 252 --test 63 --capital 100000
```

Frozen-model backtest:

```powershell
./quant_engine.exe --mode backtest --file market_data.csv --model model.json --capital 100000
```

### Tests

```powershell
./test.ps1
```

### Fetch New Data

```powershell
python fetch_data.py --ticker NVDA,AAPL --start 2022-01-01 --end 2026-07-04 --output market_data.csv
```

This downloads fresh OHLCV data from Yahoo Finance and writes it to the CSV path in `--output`. The output can replace `market_data.csv` or be written to any other filename.

### Python Export

```powershell
python train_models.py --input market_data.csv --output include/GeneratedModels.h
```

## Command Parameters

### `quant_engine.exe`

- `--mode`: `backtest`, `train`, or `predict`
- `--file`: path to the CSV input
- `--model`: path to the saved model file, usually `model.json`
- `--capital`: starting portfolio cash
- `--train`: training window size in trading days
- `--test`: test window size in trading days
- `--fft-window`: FFT lookback window used by the feature builder

### `fetch_data.py`

- `--ticker`: one ticker or a comma-separated list like `NVDA,AAPL,MSFT`
- `--start`: start date in `YYYY-MM-DD`
- `--end`: end date in `YYYY-MM-DD`
- `--output`: output CSV path
- `--tries`: retry count for failed downloads
- `--pause`: retry delay in seconds

### `train_models.py`

- `--input`: input CSV file
- `--output`: output header path for generated coefficients

## Expected Input Format

The data loader accepts either:

- Yahoo-style CSV downloaded by `yfinance`
- flat CSV with these columns:
  - `Date`
  - `Symbol`
  - `Open`
  - `High`
  - `Low`
  - `Close`
  - `Volume`
