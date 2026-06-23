import urllib.request
import os
import sys

def main():
    dest_dir = "test-images"
    os.makedirs(dest_dir, exist_ok=True)

    # 1. 通常のQRコード (Hello FileMaker)
    print("Downloading normal QR code...")
    urllib.request.urlretrieve(
        "https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=Hello+FileMaker",
        os.path.join(dest_dir, "qr_normal.png")
    )

    # 2. 日本語を含むQRコード (日本語テストメッセージ)
    print("Downloading Japanese QR code...")
    urllib.request.urlretrieve(
        "https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=%E6%97%A5%E6%9C%AC%E8%AA%9E%E3%83%86%E3%83%B3%E3%83%97%E3%83%AC%E3%83%BC%E3%83%88",
        os.path.join(dest_dir, "qr_japanese.png")
    )

    # 3. 改行を含むQRコード (Line1\nLine2\nLine3)
    print("Downloading QR code with newlines...")
    urllib.request.urlretrieve(
        "https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=Line1%0ALine2%0ALine3",
        os.path.join(dest_dir, "qr_newline.png")
    )

    print("Success: Test images saved in 'test-images/' directory.")

if __name__ == "__main__":
    main()
