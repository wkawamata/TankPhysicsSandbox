"""Compose rendered TankSandbox roll captures with numeric axis overlays."""

import argparse
import csv
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def load_trace(path):
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def roi(image, scale):
    # The automated capture camera keeps the hull at screen center. Scale 1.0
    # is the tight, tank-oriented base ROI; larger values widen it uniformly.
    side = int(min(image.width, image.height) * 0.45 * scale)
    side = min(side, image.width, image.height)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    return image.crop((left, top, left + side, top + side))


def overlay(frame, row, label, scale):
    draw = ImageDraw.Draw(frame)
    font = ImageFont.load_default()
    draw.rectangle((8, 8, 315, 82), fill=(8, 14, 24, 210), outline=(188, 208, 231))
    draw.text((16, 16), label, fill=(245, 248, 252), font=font)
    draw.text((16, 32), f"RollSign {float(row['rollSign']):+.0f}  phase {row['phase']}", fill=(210, 226, 242), font=font)
    draw.text((16, 48), f"P ({float(row['x']):+.2f}, {float(row['y']):+.2f}, {float(row['z']):+.2f}) m", fill=(210, 226, 242), font=font)
    draw.text((16, 64), f"Qz {float(row['qz']):+.3f}  ROI x{scale:.2f}", fill=(210, 226, 242), font=font)
    origin = (frame.width - 58, 58)
    draw.line((origin, (origin[0] + 36, origin[1])), fill=(240, 85, 85), width=3)
    draw.line((origin, (origin[0], origin[1] - 36)), fill=(94, 232, 130), width=3)
    draw.line((origin, (origin[0] - 24, origin[1] + 24)), fill=(80, 188, 255), width=3)
    draw.text((origin[0] + 38, origin[1] - 7), "+X", fill=(240, 85, 85), font=font)
    draw.text((origin[0] - 8, origin[1] - 50), "+Y", fill=(94, 232, 130), font=font)
    draw.text((origin[0] - 48, origin[1] + 22), "+Z", fill=(80, 188, 255), font=font)
    return frame


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--negative-dir", type=Path)
    parser.add_argument("--positive-dir", type=Path)
    parser.add_argument("--single-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--roi-scale", type=float, default=1.5)
    parser.add_argument("--duration-ms", type=int, default=66)
    args = parser.parse_args()
    if args.roi_scale < 1.0:
        raise ValueError("roi-scale must be at least 1.0")
    if args.duration_ms <= 0:
        raise ValueError("duration-ms must be positive")

    if args.single_dir:
        trace = load_trace(args.single_dir / "trace.csv")
        count = len(trace)
    elif args.negative_dir and args.positive_dir:
        negative_trace = load_trace(args.negative_dir / "trace.csv")
        positive_trace = load_trace(args.positive_dir / "trace.csv")
        count = min(len(negative_trace), len(positive_trace))
    else:
        parser.error("provide --single-dir or both --negative-dir and --positive-dir")
    if count == 0:
        raise RuntimeError("no captured frames")

    frames = []
    for index in range(count):
        if args.single_dir:
            frame = roi(
                Image.open(args.single_dir / f"roll_{index:03d}.png").convert("RGB"),
                args.roi_scale,
            )
            overlay(frame, trace[index], "Return input accepted at 75 degrees", args.roi_scale)
            frames.append(frame)
            continue
        negative = roi(Image.open(args.negative_dir / f"roll_{index:03d}.png").convert("RGB"), args.roi_scale)
        positive = roi(Image.open(args.positive_dir / f"roll_{index:03d}.png").convert("RGB"), args.roi_scale)
        overlay(negative, negative_trace[index], "Levers (-1, -1) / Roll -1", args.roi_scale)
        overlay(positive, positive_trace[index], "Levers (+1, +1) / Roll +1", args.roi_scale)
        composed = Image.new("RGB", (negative.width * 2, negative.height), (0, 0, 0))
        composed.paste(negative, (0, 0))
        composed.paste(positive, (negative.width, 0))
        frames.append(composed)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        args.output,
        save_all=True,
        append_images=frames[1:],
        duration=args.duration_ms,
        loop=0,
    )
    print(f"Saved rendered roll GIF: {args.output}")


if __name__ == "__main__":
    main()
