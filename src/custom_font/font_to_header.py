import argparse
import math
from pathlib import Path
from PIL import Image

def gen_header(
    image_file: Path,
    output_file: Path,
    var_name: str,
    num_chars: int = 95,
    start_char: int = 32,
    char_width: int = 5,
    char_height: int = 8,
    extra_padding: int = 0,
    invert: bool = False,
):
    img = Image.open(image_file).convert("1") # black or white


    assert img.width % char_width == 0, f"Image width must be a multiple of {char_width}"
    assert img.height % char_height == 0, f"Image height must be a multiple of {char_height}"

    chars_per_row = img.width // char_width
    bytes_per_col = math.ceil(char_height/8)
    data: list[int] = []

    for i in range(num_chars):
        sx = (i % chars_per_row) * char_width
        sy = (i // chars_per_row) * char_height

        for dx in range(char_width):
            x = 0
            for dy in range(char_height):
                p = int(img.getpixel((sx+dx, sy+dy)) >= 128)
                p ^= 1 if invert else 0
                x |= p << dy # highest pixel stored in LSB
            for j in range(bytes_per_col):
                data.append(x & 0xff)
                x >>= 8
    
    s = f"""// Font for {image_file.name}
#pragma once

#include <stdint.h>

/*
 * Format
 * <height>, <width>, <additional spacing per char>, 
 * <first ascii char>, <last ascii char>,
 * <data>
 */
static const uint8_t {var_name}[] = {'{'}
"""

    s += f"  {char_height}, {char_width}, {extra_padding}, {start_char}, {start_char+num_chars-1},\n"
    for i in range(num_chars):
        s += "  "
        for j in range(char_width*bytes_per_col):
            s += f"0x{data[i*char_width*bytes_per_col+j]:02x}, "
        c = chr(start_char+i)
        s += f"// {c} (0x{start_char+i:02x})\n"
    s += "};\n"

    with open(output_file, "w") as f:
        f.write(s)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("image_file")
    parser.add_argument("font_variable_name")
    parser.add_argument("-c", "--num_chars", default=95, type=int)
    parser.add_argument("-s", "--start_char", default=32, type=int)
    parser.add_argument("-W", "--char_width", default=5, type=int)
    parser.add_argument("-H", "--char_height", default=8, type=int)
    parser.add_argument("-p", "--extra_padding", default=0, type=int)
    parser.add_argument("-o", "--output_file")
    parser.add_argument("-I", "--invert", action="store_true")


    args = parser.parse_args()

    image_file = Path(args.image_file)
    output_file = image_file.with_name(image_file.name + ".h")
    if args.output_file:
        output_file = Path(args.output_file)
        output_file.absolute().parent.mkdir(exist_ok=True)

    gen_header(
        image_file,
        output_file,
        args.font_variable_name,
        args.num_chars,
        args.start_char,
        args.char_width,
        args.char_height,
        args.extra_padding,
        args.invert,
    )
