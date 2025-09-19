#!/usr/bin/env python3
"""
Simple script to create rocket icons in multiple sizes.
Creates a rocket icon with fire trail, tilted slightly right.
"""

try:
    from PIL import Image, ImageDraw
    import os
except ImportError:
    print("PIL (Pillow) is required. Install with: pip install Pillow")
    exit(1)

def create_rocket_icon(size):
    """Create a rocket icon with fire trail, tilted slightly right."""
    # Create a new image with transparency
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Calculate proportional sizes
    margin = max(1, size // 32)

    # Rocket dimensions (tilted slightly right - about 15 degrees)
    rocket_width = max(4, size // 6)
    rocket_height = max(8, size // 2)

    # Center position with slight right tilt offset
    center_x = size // 2 + size // 16  # Shift right slightly
    center_y = size // 3  # Upper portion for rocket

    # Tilt angle (15 degrees in radians)
    import math
    tilt = math.radians(15)

    def rotate_point(x, y, cx, cy, angle):
        """Rotate point around center."""
        cos_a = math.cos(angle)
        sin_a = math.sin(angle)
        x_new = cx + (x - cx) * cos_a - (y - cy) * sin_a
        y_new = cy + (x - cx) * sin_a + (y - cy) * cos_a
        return int(x_new), int(y_new)

    # Draw fire trail first (behind rocket)
    fire_length = rocket_height // 2
    fire_width = rocket_width

    # Fire particles/flames
    fire_colors = [
        (255, 100, 0, 200),   # Bright orange
        (255, 150, 0, 180),   # Orange
        (255, 200, 0, 160),   # Yellow-orange
        (255, 255, 0, 140),   # Yellow
    ]

    # Draw multiple flame shapes
    for i, color in enumerate(fire_colors):
        flame_offset = i * (fire_length // 4)
        flame_y = center_y + rocket_height // 2 + flame_offset
        flame_width = fire_width - i

        # Create flame shape points
        flame_points = [
            (center_x - flame_width // 2, flame_y),
            (center_x + flame_width // 2, flame_y),
            (center_x + flame_width // 4, flame_y + fire_length // 2),
            (center_x, flame_y + fire_length),
            (center_x - flame_width // 4, flame_y + fire_length // 2),
        ]

        # Rotate flame points
        rotated_flame = [rotate_point(x, y, center_x, center_y, tilt) for x, y in flame_points]

        if len(rotated_flame) >= 3:
            draw.polygon(rotated_flame, fill=color)

    # Draw rocket body
    rocket_points = [
        # Nose cone (top)
        (center_x, center_y - rocket_height // 2),
        (center_x - rocket_width // 4, center_y - rocket_height // 3),
        (center_x + rocket_width // 4, center_y - rocket_height // 3),

        # Main body
        (center_x - rocket_width // 2, center_y - rocket_height // 3),
        (center_x + rocket_width // 2, center_y - rocket_height // 3),
        (center_x + rocket_width // 2, center_y + rocket_height // 3),
        (center_x - rocket_width // 2, center_y + rocket_height // 3),

        # Bottom (nozzle area)
        (center_x - rocket_width // 3, center_y + rocket_height // 2),
        (center_x + rocket_width // 3, center_y + rocket_height // 2),
    ]

    # Rotate rocket points
    rotated_rocket = [rotate_point(x, y, center_x, center_y, tilt) for x, y in rocket_points]

    # Draw rocket body (main hull)
    main_body = rotated_rocket[1:7]  # Main rectangular section
    draw.polygon(main_body, fill=(200, 200, 220, 255), outline=(100, 100, 120, 255))

    # Draw nose cone
    nose_cone = rotated_rocket[0:3]
    draw.polygon(nose_cone, fill=(150, 150, 180, 255), outline=(100, 100, 120, 255))

    # Draw nozzle
    nozzle = [rotated_rocket[5], rotated_rocket[6], rotated_rocket[8], rotated_rocket[7]]
    draw.polygon(nozzle, fill=(120, 120, 140, 255), outline=(80, 80, 100, 255))

    # Add details if size is large enough
    if size >= 24:
        # Window/porthole
        window_x, window_y = rotate_point(center_x, center_y - rocket_height // 6, center_x, center_y, tilt)
        window_size = max(2, rocket_width // 4)
        draw.ellipse([window_x - window_size//2, window_y - window_size//2,
                     window_x + window_size//2, window_y + window_size//2],
                    fill=(100, 150, 255, 255), outline=(50, 100, 200, 255))

    if size >= 32:
        # Fins
        fin_size = rocket_width // 3
        # Left fin
        left_fin_base_x, left_fin_base_y = rotate_point(center_x - rocket_width//2, center_y + rocket_height//4, center_x, center_y, tilt)
        left_fin_tip_x, left_fin_tip_y = rotate_point(center_x - rocket_width//2 - fin_size, center_y + rocket_height//3, center_x, center_y, tilt)
        left_fin_bottom_x, left_fin_bottom_y = rotate_point(center_x - rocket_width//2, center_y + rocket_height//3, center_x, center_y, tilt)

        draw.polygon([
            (left_fin_base_x, left_fin_base_y),
            (left_fin_tip_x, left_fin_tip_y),
            (left_fin_bottom_x, left_fin_bottom_y)
        ], fill=(150, 150, 180, 255), outline=(100, 100, 120, 255))

        # Right fin
        right_fin_base_x, right_fin_base_y = rotate_point(center_x + rocket_width//2, center_y + rocket_height//4, center_x, center_y, tilt)
        right_fin_tip_x, right_fin_tip_y = rotate_point(center_x + rocket_width//2 + fin_size, center_y + rocket_height//3, center_x, center_y, tilt)
        right_fin_bottom_x, right_fin_bottom_y = rotate_point(center_x + rocket_width//2, center_y + rocket_height//3, center_x, center_y, tilt)

        draw.polygon([
            (right_fin_base_x, right_fin_base_y),
            (right_fin_tip_x, right_fin_tip_y),
            (right_fin_bottom_x, right_fin_bottom_y)
        ], fill=(150, 150, 180, 255), outline=(100, 100, 120, 255))

    if size >= 48:
        # Add some racing stripes or details
        stripe_y1 = rotate_point(center_x, center_y, center_x, center_y, tilt)[1]
        stripe_start_x, stripe_start_y = rotate_point(center_x - rocket_width//2 + 1, center_y, center_x, center_y, tilt)
        stripe_end_x, stripe_end_y = rotate_point(center_x + rocket_width//2 - 1, center_y, center_x, center_y, tilt)

        draw.line([stripe_start_x, stripe_start_y, stripe_end_x, stripe_end_y],
                 fill=(100, 100, 200, 255), width=max(1, size//48))

    return img

def main():
    """Create icons in multiple sizes."""
    sizes = [16, 32, 48]

    # Create icons directory if it doesn't exist
    os.makedirs('icons', exist_ok=True)

    for size in sizes:
        print(f"Creating {size}x{size} rocket icon...")
        icon = create_rocket_icon(size)

        # Save as PNG
        png_filename = f'icons/browser_{size}x{size}.png'
        icon.save(png_filename, 'PNG')
        print(f"Saved: {png_filename}")

    # Also create a multi-size ICO file for Windows
    print("Creating multi-size ICO file...")
    icons = [create_rocket_icon(size) for size in [16, 32, 48]]
    icons[0].save('icons/browser.ico', format='ICO', sizes=[(16, 16), (32, 32), (48, 48)])
    print("Saved: icons/browser.ico")

    print("Icon creation complete!")

if __name__ == '__main__':
    main()