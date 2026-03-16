#!/usr/bin/env python3
"""
Screenshot Collage Builder for UnrealClaude Timelapse Capture

Takes the JSON output from the timelapse_capture MCP tool and assembles
screenshots into an annotated grid collage for visual analysis.

Usage:
    # From timelapse JSON file:
    python screenshot_collage.py timelapse_output.json -o collage.png

    # From stdin (pipe from MCP tool):
    cat timelapse_output.json | python screenshot_collage.py - -o collage.png

    # Custom grid layout:
    python screenshot_collage.py timelapse_output.json -o collage.png --cols 5

    # With frame annotations:
    python screenshot_collage.py timelapse_output.json -o collage.png --annotate

Dependencies:
    pip install Pillow
"""

import argparse
import base64
import io
import json
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)


def load_timelapse_data(source: str) -> dict:
    """Load timelapse JSON from file or stdin."""
    if source == "-":
        data = json.load(sys.stdin)
    else:
        with open(source, "r") as f:
            data = json.load(f)

    # Handle both direct data and wrapped MCP response
    if "data" in data and "frames" in data["data"]:
        return data["data"]
    if "frames" in data:
        return data
    raise ValueError("JSON must contain a 'frames' array with 'image_base64' fields")


def decode_frame(frame: dict) -> Image.Image:
    """Decode a base64 JPEG frame to a PIL Image."""
    b64_data = frame.get("image_base64", "")
    if not b64_data:
        return None
    img_bytes = base64.b64decode(b64_data)
    return Image.open(io.BytesIO(img_bytes))


def get_font(size: int = 14):
    """Get a font, falling back to default if system fonts unavailable."""
    font_paths = [
        "C:/Windows/Fonts/consola.ttf",      # Windows Consolas
        "C:/Windows/Fonts/arial.ttf",          # Windows Arial
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",  # Linux
        "/System/Library/Fonts/Menlo.ttc",     # macOS
    ]
    for path in font_paths:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size)
            except (OSError, IOError):
                continue
    return ImageFont.load_default()


def build_collage(
    data: dict,
    cols: int = 5,
    annotate: bool = True,
    padding: int = 4,
    header_height: int = 24,
    bg_color: tuple = (30, 30, 30),
    text_color: tuple = (255, 255, 255),
    accent_color: tuple = (0, 200, 100),
) -> Image.Image:
    """Build a grid collage from timelapse frames."""

    frames = data.get("frames", [])
    if not frames:
        raise ValueError("No frames in timelapse data")

    # Decode all frames
    images = []
    for frame in frames:
        img = decode_frame(frame)
        if img:
            images.append((img, frame))

    if not images:
        raise ValueError("No valid images decoded from frames")

    # Calculate grid dimensions
    num_frames = len(images)
    rows = (num_frames + cols - 1) // cols
    frame_w, frame_h = images[0][0].size

    # Calculate total canvas size
    cell_w = frame_w + padding
    cell_h = frame_h + (header_height if annotate else 0) + padding
    canvas_w = cols * cell_w + padding
    canvas_h = rows * cell_h + padding

    # Add title bar
    title_height = 40 if annotate else 0
    canvas_h += title_height

    # Create canvas
    canvas = Image.new("RGB", (canvas_w, canvas_h), bg_color)
    draw = ImageDraw.Draw(canvas)

    font_small = get_font(12)
    font_title = get_font(16)

    # Draw title
    if annotate:
        interval_ms = data.get("interval_ms", "?")
        total_time = data.get("total_time_seconds", 0)
        title_text = (
            f"Timelapse: {num_frames} frames | "
            f"{interval_ms}ms interval | "
            f"{total_time:.1f}s total | "
            f"{frame_w}x{frame_h}"
        )
        draw.text((padding + 4, 10), title_text, fill=accent_color, font=font_title)

    # Place frames
    for idx, (img, frame) in enumerate(images):
        row = idx // cols
        col = idx % cols

        x = padding + col * cell_w
        y = title_height + padding + row * cell_h

        # Draw annotation header
        if annotate:
            timestamp_ms = frame.get("timestamp_ms", 0)
            game_time = frame.get("game_time", 0)
            speed = frame.get("player_speed", 0)

            label = f"#{idx}  {timestamp_ms:.0f}ms  GT:{game_time:.1f}s"
            if speed > 0:
                label += f"  spd:{speed:.0f}"

            draw.text((x + 4, y + 2), label, fill=text_color, font=font_small)
            y += header_height

        # Paste frame image
        canvas.paste(img, (x, y))

        # Draw thin border around frame
        draw.rectangle(
            [x - 1, y - 1, x + frame_w, y + frame_h],
            outline=(60, 60, 60),
            width=1,
        )

    return canvas


def main():
    parser = argparse.ArgumentParser(
        description="Build screenshot collage from UnrealClaude timelapse capture"
    )
    parser.add_argument(
        "input",
        help="Path to timelapse JSON file, or '-' for stdin",
    )
    parser.add_argument(
        "-o", "--output",
        default="collage.png",
        help="Output image path (default: collage.png)",
    )
    parser.add_argument(
        "--cols",
        type=int,
        default=5,
        help="Number of columns in the grid (default: 5)",
    )
    parser.add_argument(
        "--no-annotate",
        action="store_true",
        help="Disable frame annotations (timestamps, game time)",
    )
    parser.add_argument(
        "--padding",
        type=int,
        default=4,
        help="Padding between frames in pixels (default: 4)",
    )

    args = parser.parse_args()

    # Load data
    print(f"Loading timelapse data from: {args.input}")
    data = load_timelapse_data(args.input)

    frame_count = len(data.get("frames", []))
    print(f"Found {frame_count} frames")

    # Build collage
    collage = build_collage(
        data,
        cols=args.cols,
        annotate=not args.no_annotate,
        padding=args.padding,
    )

    # Save
    output_path = Path(args.output)
    collage.save(output_path, quality=90)
    print(f"Collage saved: {output_path} ({collage.size[0]}x{collage.size[1]})")


if __name__ == "__main__":
    main()
