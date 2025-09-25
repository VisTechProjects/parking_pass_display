from pathlib import Path
from typing import List, Dict, Optional
from PIL import Image, ImageOps
from pyzbar.pyzbar import decode, ZBarSymbol

ALLOWED_EXTS = [".pdf", ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"]

def render_pdf_with_pdfium(pdf_path: Path, dpi: int = 300) -> Optional[List[Image.Image]]:
    try:
        import pypdfium2 as pdfium
    except Exception:
        return None
    pdf = pdfium.PdfDocument(str(pdf_path))
    images = []
    scale = dpi / 72.0  # 72 dpi is 1.0
    for i in range(len(pdf)):
        page = pdf[i]
        bitmap = page.render(scale=scale)
        images.append(bitmap.to_pil())
    return images

def render_pdf_with_pdf2image(pdf_path: Path, dpi: int = 300) -> Optional[List[Image.Image]]:
    try:
        from pdf2image import convert_from_path
    except Exception:
        return None
    try:
        return convert_from_path(str(pdf_path), dpi=dpi, use_pdfium=True)
    except TypeError:
        try:
            return convert_from_path(str(pdf_path), dpi=dpi)
        except Exception:
            return None
    except Exception:
        return None

def load_images(path: Path) -> List[Image.Image]:
    if path.suffix.lower() == ".pdf":
        imgs = render_pdf_with_pdfium(path, dpi=300)
        if imgs:
            return imgs
        imgs = render_pdf_with_pdf2image(path, dpi=300)
        if imgs:
            return imgs
        raise SystemExit(
            "Could not render PDF. Install either:\n"
            "  pip install pypdfium2   (no system deps)\n"
            "or ensure Poppler is installed & on PATH for pdf2image.\n"
        )
    else:
        img = Image.open(path)
        frames = []
        try:
            while True:
                frames.append(img.copy())
                img.seek(len(frames))
        except EOFError:
            pass
        return frames or [Image.open(path)]

def zbar_symbol_list():
    """Return a robust list of ZBar symbols supported by the current build."""
    base = [
        getattr(ZBarSymbol, "CODE39", None),
        getattr(ZBarSymbol, "CODE128", None),
        getattr(ZBarSymbol, "CODE93", None),
        getattr(ZBarSymbol, "EAN13", None),
        getattr(ZBarSymbol, "EAN8", None),
        getattr(ZBarSymbol, "UPCA", None),
        getattr(ZBarSymbol, "UPCE", None),
        getattr(ZBarSymbol, "QRCODE", None),   # not needed for permit, but harmless
        getattr(ZBarSymbol, "PDF417", None),   # likewise
    ]
    # Some builds name Interleaved 2 of 5 as I25 (not ITF)
    i25 = getattr(ZBarSymbol, "I25", None) or getattr(ZBarSymbol, "ITF", None)
    if i25:
        base.append(i25)
    return [s for s in base if s is not None]

SYMBOLS = zbar_symbol_list()

def try_decode(img: Image.Image) -> List[Dict]:
    results = []
    base = ImageOps.grayscale(img)

    for scale in (1.0, 1.5, 2.0):
        w = int(base.width * scale); h = int(base.height * scale)
        work = base.resize((w, h), Image.BICUBIC)

        dec = decode(work, symbols=SYMBOLS)
        for d in dec:
            try:
                data = d.data.decode("utf-8", errors="ignore")
            except Exception:
                data = str(d.data)
            bbox = {"x": d.rect.left, "y": d.rect.top, "w": d.rect.width, "h": d.rect.height}
            results.append({"type": d.type, "value": data, "bbox": bbox})
        if results:
            break

    uniq, seen = [], set()
    for r in results:
        k = (r["type"], r["value"])
        if k not in seen:
            seen.add(k); uniq.append(r)
    return uniq

def pick_file(folder: Path) -> Optional[Path]:
    # p = folder / "permit.pdf"
    p = folder / "library_no_checksum.png"
    if p.exists():
        return p
    cands = [f for f in folder.iterdir() if f.is_file() and f.suffix.lower() in ALLOWED_EXTS]
    if not cands:
        return None
    return max(cands, key=lambda f: f.stat().st_mtime)

def main():
    here = Path(__file__).resolve().parent
    target = pick_file(here)
    if not target:
        print("No permit file found in the same folder. Supported: " + ", ".join(ALLOWED_EXTS))
        return

    print(f"Scanning: {target.name}")
    pages = load_images(target)
    any_hits = False
    for i, page in enumerate(pages, start=1):
        hits = try_decode(page)
        for h in hits:
            any_hits = True
            print(f"[page {i}] {h['type']}: {h['value']}  (bbox {h['bbox']})")
    if not any_hits:
        print("No barcodes found. Try higher DPI render or crop to barcode region.")

if __name__ == "__main__":
    main()