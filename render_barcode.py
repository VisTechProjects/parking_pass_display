from pathlib import Path
from barcode.codex import Code39
from barcode.writer import ImageWriter

def generate_code39_no_checksum(payload: str, out_png: str = "code39_no_checksum.png") -> str:
    # Create Code39 with NO checksum
    code = Code39(payload, writer=ImageWriter(), add_checksum=False)

    # Tune bars at the source (never resize after save)
    options = {
        "module_width": 0.6,   # narrow bar width in "writer units" (~px at 300dpi)
        "module_height": 40.0, # bar height
        "quiet_zone": 6.0,     # ~10 modules is ideal; adjust with module_width too
        "write_text": False,   # we'll draw digits separately if needed
        "font_size": 0,        # suppress library text
        "text_distance": 0,
        "dpi": 300,
        "background": "white",
        "foreground": "black",
    }

    base = Path(out_png).with_suffix("")  # python-barcode appends .png
    filename = code.save(str(base), options=options)
    print(f"Saved: {filename} (Code 39, no checksum, payload={payload})")
    return filename

if __name__ == "__main__":
    permit_numeric = "6103268"  # what scanners read from the official permit
    generate_code39_no_checksum(permit_numeric, "library_no_checksum.png")
