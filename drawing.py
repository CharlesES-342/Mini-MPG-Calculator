def text_to_bitmap(ascii_art):
    lines = [line.strip() for line in ascii_art.strip().split('\n') if line.strip()]
    height = len(lines)
    width = len(lines[0])
    
    print( f"Bitmap size: {width}x{height}px")
    print("const unsigned char my_custom_bmp[] PROGMEM = {")
    
    for row_idx, line in enumerate(lines):
        byte_list = []
        # Process every 8 pixels (1 byte)
        for i in range(0, width, 8):
            chunk = line[i:i+8]
            byte_val = 0
            for bit_idx, char in enumerate(chunk):
                if char == '#':  # # means pixel ON (white)
                    byte_val |= (1 << (7 - bit_idx))
            byte_list.append(f"0x{byte_val:02X}")
        
        # Format the row for C++
        row_str = ", ".join(byte_list)
        end_comma = "," if row_idx < height - 1 else ""
        print(f"\t{row_str}{end_comma} // Row {row_idx}")
        
    print("};")

# Example: Draw your graphic visually here using '#' and '.'
my_drawing = """
................................
.....#######....................
....#.....##....................
....#.....##########............
....#.##..##...##..#............
....##..#######..###............
......##.......##...............
......##.......##...............
"""

text_to_bitmap(my_drawing)