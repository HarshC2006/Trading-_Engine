from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd


FEATURE_COUNT = 16
FFT_WINDOW = 16
LOGISTIC_L2 = 0.05
RIDGE_L2 = 5.0
LOGISTIC_STEPS = 800
LOGISTIC_LR = 0.05
BUY_LEVEL = 0.55
SELL_LEVEL = 0.45
MIN_MOVE = 0.0010
EPS = 1e-9


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train logistic and ridge models, then export a C++ header.")
    parser.add_argument("--input", default="market_data.csv", help="Market data CSV.")
    parser.add_argument("--output", default="include/GeneratedModels.h", help="Header output path.")
    return parser.parse_args()


def load_bars(path: str) -> pd.DataFrame:
    raw = Path(path).read_text(encoding="utf-8").splitlines()
    raw = [line for line in raw if line.strip()]
    if not raw:
        raise ValueError(f"empty file: {path}")

    if raw[0].startswith("Price,") and len(raw) >= 4 and raw[1].startswith("Ticker,"):
        symbol = raw[1].split(",")[1].strip() or "ASSET"
        rows = []
        for line in raw[3:]:
            parts = line.split(",")
            if len(parts) < 6:
                continue
            try:
                rows.append(
                    {
                        "Date": parts[0],
                        "Symbol": symbol,
                        "Open": float(parts[4]),
                        "High": float(parts[2]),
                        "Low": float(parts[3]),
                        "Close": float(parts[1]),
                        "Volume": float(parts[5]),
                    }
                )
            except ValueError:
                continue
        frame = pd.DataFrame(rows)
    else:
        frame = pd.read_csv(path)
        frame.columns = [col.strip() for col in frame.columns]
        name_map = {col.lower(): col for col in frame.columns}
        needed = ["date", "open", "high", "low", "close", "volume"]
        if any(name not in name_map for name in needed):
            raise ValueError("csv must contain Date/Open/High/Low/Close/Volume columns")

        frame = frame.rename(
            columns={
                name_map["date"]: "Date",
                name_map["open"]: "Open",
                name_map["high"]: "High",
                name_map["low"]: "Low",
                name_map["close"]: "Close",
                name_map["volume"]: "Volume",
                name_map.get("symbol", ""): "Symbol",
                name_map.get("ticker", ""): "Symbol",
            }
        )
        if "Symbol" not in frame.columns:
            frame["Symbol"] = "ASSET"

    frame["Date"] = frame["Date"].astype(str)
    frame["Symbol"] = frame["Symbol"].astype(str)
    frame = frame.sort_values(["Date", "Symbol"]).reset_index(drop=True)
    return frame[["Date", "Symbol", "Open", "High", "Low", "Close", "Volume"]]


def spectral_stats(rets: np.ndarray, end_idx: int, window: int) -> tuple[float, float, float, float, float]:
    frame = rets[end_idx + 1 - window : end_idx + 1].copy()
    frame -= frame.mean()

    low = 0.0
    mid = 0.0
    high = 0.0
    for k in range(1, (window // 2) + 1):
        angle = np.exp(-2j * np.pi * k * np.arange(window) / window)
        energy = abs(np.dot(frame, angle)) ** 2
        if k <= 2:
            low += energy
        elif k <= 5:
            mid += energy
        else:
            high += energy

    total = low + mid + high + EPS
    return (
        low / total,
        mid / total,
        high / total,
        low / (abs(mid) + EPS),
        high / (abs(low) + EPS),
    )


def build_samples(bars: pd.DataFrame, fft_window: int = FFT_WINDOW) -> pd.DataFrame:
    rows: list[dict[str, float | str]] = []
    lookback = max(20, fft_window)

    for symbol, frame in bars.groupby("Symbol", sort=False):
        frame = frame.sort_values("Date").reset_index(drop=True)
        if len(frame) <= lookback + 1:
            continue

        close = frame["Close"].to_numpy(dtype=float)
        open_ = frame["Open"].to_numpy(dtype=float)
        high = frame["High"].to_numpy(dtype=float)
        low = frame["Low"].to_numpy(dtype=float)
        volume = frame["Volume"].to_numpy(dtype=float)
        dates = frame["Date"].astype(str).to_numpy()

        rets = np.zeros(len(frame), dtype=float)
        rets[1:] = close[1:] / close[:-1] - 1.0

        tr = np.zeros(len(frame), dtype=float)
        prev_close = np.roll(close, 1)
        prev_close[0] = close[0]
        tr = np.maximum.reduce([high - low, np.abs(high - prev_close), np.abs(low - prev_close)])

        for i in range(lookback, len(frame) - 1):
            mean3 = rets[i + 1 - 3 : i + 1].mean()
            mean5 = rets[i + 1 - 5 : i + 1].mean()
            mean10 = rets[i + 1 - 10 : i + 1].mean()
            mean20 = rets[i + 1 - 20 : i + 1].mean()
            vol5 = rets[i + 1 - 5 : i + 1].std(ddof=1)
            vol10 = rets[i + 1 - 10 : i + 1].std(ddof=1)
            vol20 = rets[i + 1 - 20 : i + 1].std(ddof=1)
            atr14 = tr[i + 1 - 14 : i + 1].mean() / max(close[i], EPS)
            day_range = (high[i] - low[i]) / max(close[i], EPS)
            vol_mean = volume[i + 1 - 20 : i + 1].mean()
            vol_std = volume[i + 1 - 20 : i + 1].std(ddof=1)
            vol_z = (volume[i] - vol_mean) / (vol_std + EPS)
            band_low, band_mid, band_high, low_mid, high_low = spectral_stats(rets, i, fft_window)

            rows.append(
                {
                    "date": dates[i],
                    "symbol": symbol,
                    "next_date": dates[i + 1],
                    "close": close[i],
                    "next_open": open_[i + 1],
                    "next_ret": close[i + 1] / max(open_[i + 1], EPS) - 1.0,
                    "atr": atr14,
                    "vol": vol20,
                    "f0": rets[i],
                    "f1": mean3,
                    "f2": mean5,
                    "f3": mean10,
                    "f4": mean20,
                    "f5": vol5,
                    "f6": vol10,
                    "f7": vol20,
                    "f8": atr14,
                    "f9": day_range,
                    "f10": band_low,
                    "f11": band_mid,
                    "f12": band_high,
                    "f13": low_mid,
                    "f14": high_low,
                    "f15": vol_z,
                }
            )

    if not rows:
        raise ValueError("not enough rows to build features")

    return pd.DataFrame(rows).sort_values(["date", "symbol"]).reset_index(drop=True)


@dataclass
class Standardizer:
    mean_: np.ndarray
    scale_: np.ndarray

    @classmethod
    def fit(cls, x: np.ndarray) -> "Standardizer":
        mean = x.mean(axis=0)
        scale = x.std(axis=0, ddof=1)
        scale[scale < EPS] = 1.0
        return cls(mean, scale)

    def transform(self, x: np.ndarray) -> np.ndarray:
        return (x - self.mean_) / self.scale_


@dataclass
class LogisticRegression:
    scaler: Standardizer
    coef_: np.ndarray
    intercept_: float

    @classmethod
    def fit(cls, x: np.ndarray, y: np.ndarray) -> "LogisticRegression":
        scaler = Standardizer.fit(x)
        z = scaler.transform(x)
        coef = np.zeros(z.shape[1], dtype=float)
        intercept = 0.0

        for _ in range(LOGISTIC_STEPS):
            score = z @ coef + intercept
            prob = 1.0 / (1.0 + np.exp(-score))
            err = prob - y
            grad = (z.T @ err) / len(z) + LOGISTIC_L2 * coef
            grad_b = err.mean()
            coef -= LOGISTIC_LR * grad
            intercept -= LOGISTIC_LR * grad_b

        return cls(scaler, coef, float(intercept))


@dataclass
class RidgeRegression:
    scaler: Standardizer
    coef_: np.ndarray
    intercept_: float

    @classmethod
    def fit(cls, x: np.ndarray, y: np.ndarray) -> "RidgeRegression":
        scaler = Standardizer.fit(x)
        z = scaler.transform(x)
        design = np.column_stack([np.ones(len(z)), z])
        eye = np.eye(design.shape[1])
        eye[0, 0] = 0.0
        beta = np.linalg.solve(design.T @ design + RIDGE_L2 * eye, design.T @ y)
        return cls(scaler, beta[1:], float(beta[0]))


def fmt_array(values: np.ndarray) -> str:
    items = []
    for idx, value in enumerate(values):
        text = f"{float(value):.12g}"
        items.append(text)
        if idx + 1 < len(values):
            items.append(", ")
        if (idx + 1) % 4 == 0 and idx + 1 < len(values):
            items.append("\n    ")
    return "".join(items)


def write_header(path: str, logit: LogisticRegression, ridge: RidgeRegression) -> None:
    header = f"""#pragma once

#include "Features.h"

#include <array>

namespace generated {{

constexpr std::array<double, kFeatureCount> logistic_mean = {{
    {fmt_array(logit.scaler.mean_)}
}};
constexpr std::array<double, kFeatureCount> logistic_scale = {{
    {fmt_array(logit.scaler.scale_)}
}};
constexpr std::array<double, kFeatureCount> logistic_weights = {{
    {fmt_array(logit.coef_)}
}};
constexpr double logistic_bias = {logit.intercept_:.12g};

constexpr std::array<double, kFeatureCount> ridge_mean = {{
    {fmt_array(ridge.scaler.mean_)}
}};
constexpr std::array<double, kFeatureCount> ridge_scale = {{
    {fmt_array(ridge.scaler.scale_)}
}};
constexpr std::array<double, kFeatureCount> ridge_weights = {{
    {fmt_array(ridge.coef_)}
}};
constexpr double ridge_bias = {ridge.intercept_:.12g};

constexpr double buy_level = {BUY_LEVEL:.12g};
constexpr double sell_level = {SELL_LEVEL:.12g};
constexpr double min_move = {MIN_MOVE:.12g};

}}
"""
    Path(path).write_text(header, encoding="utf-8")


def main() -> int:
    args = parse_args()
    bars = load_bars(args.input)
    samples = build_samples(bars)

    feature_cols = [f"f{i}" for i in range(FEATURE_COUNT)]
    x = samples[feature_cols].to_numpy(dtype=float)
    y_cls = (samples["next_ret"].to_numpy(dtype=float) > 0.0).astype(float)
    y_reg = samples["next_ret"].to_numpy(dtype=float)

    logit = LogisticRegression.fit(x, y_cls)
    ridge = RidgeRegression.fit(x, y_reg)
    write_header(args.output, logit, ridge)

    prob = 1.0 / (1.0 + np.exp(-(logit.scaler.transform(x) @ logit.coef_ + logit.intercept_)))
    pred = ridge.scaler.transform(x) @ ridge.coef_ + ridge.intercept_
    acc = float(((prob >= 0.5) == y_cls).mean())
    mse = float(np.mean((pred - y_reg) ** 2))

    print(f"samples: {len(samples)}")
    print(f"logit_acc: {acc:.4f}")
    print(f"ridge_mse: {mse:.8f}")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
