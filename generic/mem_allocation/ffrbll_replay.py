from math import ceil
import argparse
import re
import sys

import numpy
from PIL import Image, ImageDraw, ImageFont


def parse_events(replay_file_contents):
    """Parse the events from the contents of a file.

    Args:
        replay_file_contents(str): Replay file contents.

    Returns:
        list(tuple[str, int, int]): Mode (+/-), offset, size
    """
    return [
        (mode, int(offset), int(size))
        for mode, offset, size in re.findall(
            r"^([+-]) (-?\d+) (-?\d+)$",
            replay_file_contents,
            flags=re.MULTILINE
        )
    ]


def calc_bounds(events):
    """Calculate the min/max offset bounds of parsed events.
    
    Args:
        events(list(tuple[str, int, int])): Mode (+/-), offset, size

    Returns:
        tuple(int, int): Min, Max offsets
    """
    min_offset = float("inf")
    max_offset = float("-inf")
    for _, offset, size in events:
        if offset < min_offset:
            min_offset = offset
        if offset + size > max_offset:
            max_offset = offset + size
    return min_offset, max_offset


def replay_iterate(replay_file_contents):
    """Parse a replay files contents and iterate a image for each event.

    Args:
        replay_file_contents(str): Replay file contents.

    Yields:
        PIL.Image: Generated image for the current event.
    """
    events = parse_events(replay_file_contents)

    if not events:
        return

    bounds = calc_bounds(events)
    allocations_size = bounds[1] - bounds[0]

    # Our final image is a square, with extra pixels as unallocated padding
    width = max(32, int(ceil(allocations_size**0.5)))
    height = int(ceil(allocations_size/width))
    padding_pixels = ((width*height) - allocations_size)

    # Make each allocation between 1=>50 square pixels
    # Allocations > 4GB are simply going to be represented by a pixel
    cell_size = max(1, min(10, int(2048 / width)))
    allocated_2d = numpy.zeros((height, width), dtype=int)
    # Linear representation of the image that we can write into directly
    allocated_dma = allocated_2d.ravel()

    font = ImageFont.load_default()

    # Gray-scale background
    background = 190
    used_colour = (25, 25, 127)
    free_colour = (24, 190, 24)
    invalid_colour = (255, 0, 0)

    for idx, (mode, offset, size) in enumerate(events):

        allocated_dma[offset - bounds[0]: offset + size - bounds[0]] += 1 if mode == "+" else -1

        is_free = allocated_2d == 1
        invalid = (allocated_2d < 0) | (allocated_2d > 1)
        image = numpy.full((height, width, 3), used_colour, dtype=numpy.uint8)

        if padding_pixels:
            image.ravel()[-3*(padding_pixels):] = background

        image[is_free] = free_colour
        image[invalid] = invalid_colour

        if cell_size > 1:
            image = numpy.kron(image, numpy.ones((cell_size, cell_size, 1), dtype=numpy.uint8))
        
        idx_text = "Event ID: {0}".format(idx)
        percent_free_text = "Free: {0:.2%}".format(is_free.sum() / allocations_size)
        operation_text = "{0} 0x{1:08x} : {2}".format(
            "free " if mode == "+" else "alloc",
            offset,
            size
        )
        
        margin = 10
        text_height = 10
        text_margin = 2
        
        frame = Image.new(
            "RGB",
            (
                width * cell_size + margin * 2,
                height * cell_size
                # Extra margin between text and allocations
                + margin * 3
                + text_height * 3
                + text_margin * 2
            ),
            (background, background, background)
        )

        frame.paste(Image.fromarray(image), (margin, margin))

        ctx = ImageDraw.Draw(frame)

        text_start = (
            margin,
            margin * 2 + height * cell_size
        )

        # Event id
        ctx.text(
            (text_start[0], text_start[1] + (text_height + text_margin)*0),
            idx_text,
            (0,0,0),
            font=font
        )

        # Free percentage
        ctx.text(
            (text_start[0], text_start[1] + (text_height + text_margin)*1),
            percent_free_text,
            (0,0,0),
            font=font
        )

        # Operation
        ctx.text(
            (text_start[0], text_start[1] + (text_height + text_margin)*2),
            operation_text,
            (0,0,0),
            font=font
        )

        yield frame.convert('P', palette=Image.ADAPTIVE)

        sys.stdout.write(
            "\r{0:.2%}\t[{1}/{2}]".format(
                (idx+1) / len(events),
                (idx+1),
                len(events)
            )
        )
        sys.stdout.flush()

    sys.stdout.write("\nALL DONE\n")
    sys.stdout.flush()


def replay_to_gif(replay_file_contents, out_gif):
    """Given a replay files contents, generate a gif of the timeline of events.

    Args:
        replay_file_contents(str): Replay file contents.
        out_gif(str): Where to save the gif.
    """
    images = list(replay_iterate(replay_file_contents))
    if images:
        images[0].save(
            out_gif,
            save_all=True,
            append_images=images[1:],
            optimize=False,
            duration=1,
            loop=0
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser();
    parser.add_argument("-i", "--input-replay-file", required=True)
    parser.add_argument("-o", "--output-gif", required=True)
    args = parser.parse_args()

    with open(args.input_replay_file, "r") as in_fp:
        replay_to_gif(in_fp.read(), args.output_gif)
