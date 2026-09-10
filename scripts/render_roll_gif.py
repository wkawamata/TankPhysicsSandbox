"""Render deterministic lever-input roll traces as an animated GIF."""

import argparse
import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


WIDTH = 900
HEIGHT = 430
BACKGROUND = (19, 25, 35)
GROUND = (110, 128, 146)
CHASSIS = (208, 175, 60)
TRACK = (70, 180, 214)
TEXT = (238, 243, 250)
MUTED = (166, 180, 196)


def model_roll_angle_degrees(row, prefix):
    # Roll about model +Z. The quaternion is copied without coordinate conversion
    # from the physics trace, matching the project's shared axis contract.
    qz = float(row[f"{prefix}_qz"])
    qw = float(row[f"{prefix}_qw"])
    return math.degrees(2.0 * math.atan2(qz, qw))


def draw_vehicle(draw, center, angle_degrees, pixels_per_meter):
    width = 5.26 * pixels_per_meter
    height = 2.60 * pixels_per_meter
    radians = math.radians(angle_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    corners = []
    for local_x, local_y in ((-width / 2, -height / 2), (width / 2, -height / 2),
                             (width / 2, height / 2), (-width / 2, height / 2)):
        corners.append((
            center[0] + local_x * cosine - local_y * sine,
            center[1] + local_x * sine + local_y * cosine,
        ))
    draw.polygon(corners, fill=CHASSIS, outline=TEXT, width=3)

    # Model +Z (front) is shown by the cyan triangle on the vehicle's right edge.
    front = (width / 2, 0)
    nose = (width / 2 + 24, 0)
    upper = (width / 2 - 7, -14)
    lower = (width / 2 - 7, 14)
    triangle = []
    for local_x, local_y in (nose, upper, lower):
        triangle.append((
            center[0] + local_x * cosine - local_y * sine,
            center[1] + local_x * sine + local_y * cosine,
        ))
    draw.polygon(triangle, fill=TRACK)


def render_frame(row, frame_index, total_frames, roi_scale):
    image = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    title = "Lever-input rolling test  |  Model/World: LH +X right, +Y up, +Z front"
    draw.text((20, 15), title, fill=TEXT, font=font)

    panel_width = WIDTH // 2
    # 1.0 is the rotating vehicle's tight square envelope. The camera follows
    # its center; higher values numerically widen the view around the tank.
    rotating_envelope_meters = math.hypot(5.26, 2.60)
    roi_span_meters = rotating_envelope_meters * roi_scale
    pixels_per_meter = min(
        (panel_width - 52.0) / roi_span_meters,
        (HEIGHT - 172.0) / roi_span_meters,
    )

    for index, (prefix, lever_sign) in enumerate((("negative", -1), ("positive", 1))):
        panel_left = index * panel_width
        center_x = panel_left + panel_width // 2
        center_y = 270
        x = float(row[f"{prefix}_x"])
        y = float(row[f"{prefix}_y"])
        # The ROI follows the hull center. World X/Y are still shown as data,
        # while the visible ground offset preserves the contact relationship.
        center = (center_x, center_y)
        ground_y = center_y + y * pixels_per_meter
        angle = model_roll_angle_degrees(row, prefix)

        draw.rectangle((panel_left + 8, 55, panel_left + panel_width - 8, HEIGHT - 12),
                       outline=(57, 73, 91), width=2)
        draw.line((panel_left + 20, ground_y, panel_left + panel_width - 20, ground_y),
                  fill=GROUND, width=3)
        draw.line((center_x, 82, center_x, HEIGHT - 25), fill=(54, 70, 87), width=1)
        draw.text((panel_left + 24, 75),
                  f"Levers ({lever_sign:+d}, {lever_sign:+d})  RollSign {lever_sign:+d}",
                  fill=TEXT, font=font)
        draw.text((panel_left + 24, 95),
                  f"model-Z rotation: {angle:+.1f} deg", fill=MUTED, font=font)
        draw.text((panel_left + 24, 113),
                  f"phase: {row[f'{prefix}_phase']}", fill=MUTED, font=font)
        draw.text((panel_left + 24, 131), "cyan arrow = model +Z front", fill=MUTED, font=font)
        draw.text((panel_left + 24, 149),
                  f"world X: {x:+.2f} m   ROI scale: {roi_scale:.2f}",
                  fill=MUTED, font=font)
        draw.text((panel_left + 24, HEIGHT - 48), "+X", fill=MUTED, font=font)
        draw.text((panel_left + panel_width - 48, HEIGHT - 48), "-X", fill=MUTED, font=font)
        draw_vehicle(draw, center, angle, pixels_per_meter)

    draw.text((20, HEIGHT - 28),
              f"physics frame {frame_index + 1}/{total_frames}  (60 Hz)",
              fill=MUTED, font=font)
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--roi-scale", default=1.5, type=float)
    args = parser.parse_args()

    with args.input.open(newline="", encoding="utf-8") as trace_file:
        rows = list(csv.DictReader(trace_file))
    if not rows:
        raise RuntimeError("roll trace has no frames")

    if args.roi_scale < 1.0:
        raise ValueError("roi-scale must be at least 1.0")
    frames = [
        render_frame(row, index, len(rows), args.roi_scale)
        for index, row in enumerate(rows)
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        args.output,
        save_all=True,
        append_images=frames[1:],
        duration=1000 // 30,
        loop=0,
        optimize=False,
    )
    print(f"Saved GIF: {args.output}")


if __name__ == "__main__":
    main()
