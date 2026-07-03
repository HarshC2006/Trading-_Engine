from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import pandas as pd
import yfinance as yf


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Download OHLCV data from Yahoo Finance.")
    parser.add_argument("--ticker", required=True, help="Ticker or comma-separated list of tickers.")
    parser.add_argument("--start", required=True, help="Start date in YYYY-MM-DD format.")
    parser.add_argument("--end", required=True, help="End date in YYYY-MM-DD format.")
    parser.add_argument("--output", required=True, help="Output CSV path.")
    parser.add_argument("--tries", type=int, default=3, help="Retry count per ticker.")
    parser.add_argument("--pause", type=float, default=1.5, help="Seconds between retries.")
    return parser.parse_args()


def norm_name(name: object) -> str:
    if isinstance(name, tuple):
        parts = [str(part).strip() for part in name if str(part).strip()]
        if len(parts) >= 2 and parts[0].lower() == "date":
            return "Date"
        if parts:
            return parts[0]
        return ""
    return str(name).strip()


def flatten_columns(df: pd.DataFrame) -> pd.DataFrame:
    out = df.copy()
    out.columns = [norm_name(name) for name in out.columns]
    return out


def fetch_one(ticker: str, start: str, end: str, tries: int, pause: float) -> pd.DataFrame:
    last_error: Exception | None = None
    for attempt in range(1, tries + 1):
        try:
            df = yf.download(
                ticker,
                start=start,
                end=end,
                auto_adjust=False,
                progress=False,
                threads=False,
            )
            if df.empty:
                raise ValueError(f"no data returned for {ticker}")

            df = flatten_columns(df.reset_index())
            cols = {name.lower(): name for name in df.columns}
            close_name = "Adj Close" if "adj close" in cols else "Close"

            out = (
                df[["Date", "Open", "High", "Low", close_name, "Volume"]]
                .rename(columns={close_name: "Close"})
                .assign(Symbol=ticker.upper())
            )
            return out[["Date", "Symbol", "Open", "High", "Low", "Close", "Volume"]]
        except Exception as exc:  # pragma: no cover - network and API failures vary
            last_error = exc
            if attempt == tries:
                break
            time.sleep(pause)
    raise RuntimeError(f"failed to download {ticker}: {last_error}") from last_error


def main() -> int:
    args = parse_args()
    tickers = [part.strip().upper() for part in args.ticker.split(",") if part.strip()]
    if not tickers:
        raise ValueError("at least one ticker is required")

    frames = [fetch_one(ticker, args.start, args.end, args.tries, args.pause) for ticker in tickers]
    data = pd.concat(frames, ignore_index=True).sort_values(["Date", "Symbol"]).reset_index(drop=True)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    data.to_csv(output, index=False)

    print(f"saved {len(data)} rows for {len(tickers)} ticker(s) to {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
