"""Exercise alpha, escape bytes and scanline boundaries used by Switch UI assets."""
import struct
import unittest

from PIL import Image
from convert_ui_textures import decode_pcx, encode_pcx


class TextureCodecTests(unittest.TestCase):
    def test_known_planar_bytes_and_alpha(self):
        image = Image.frombytes("RGBA", (2, 1), bytes((0x12, 0x34, 0x56, 0xff, 0xc1, 0x34, 0x78, 0)))
        encoded = encode_pcx(image)
        # Independently specified R, G, B, A scanlines; high bytes must escape.
        self.assertEqual(encoded[128:], bytes((0x12, 0xc1, 0xc1, 0xc2, 0x34, 0x56, 0x78, 0xc1, 0xff, 0)))
        self.assertEqual(decode_pcx(encoded).tobytes(), image.tobytes())

    def test_odd_width_plane_padding(self):
        image = Image.frombytes("RGBA", (1, 1), bytes((1, 2, 3, 4)))
        encoded = encode_pcx(image)
        self.assertEqual(struct.unpack_from("<H", encoded, 66)[0], 2)
        self.assertEqual(encoded[128:], bytes((1, 0, 2, 0, 3, 0, 4, 0)))
        self.assertEqual(decode_pcx(encoded).tobytes(), bytes((1, 2, 3, 4)))

    def test_maximum_run_stops_at_plane_boundary(self):
        encoded = encode_pcx(Image.new("RGBA", (64, 2), (255, 255, 255, 255)))
        self.assertEqual(encoded[128:], bytes((0xff, 0xff, 0xc1, 0xff)) * 8)

    def test_transparent_override(self):
        image = decode_pcx(encode_pcx(Image.new("RGBA", (1, 1), (0, 0, 0, 0))))
        self.assertEqual(image.getpixel((0, 0)), (0, 0, 0, 0))

    def test_malformed_input_rejected(self):
        encoded = encode_pcx(Image.new("RGBA", (2, 1), (1, 2, 3, 4)))
        for invalid in (encoded[:127], encoded[:-1], encoded + b"\x00", encoded[:128] + b"\xc0\x00"):
            with self.subTest(length=len(invalid)), self.assertRaises(ValueError):
                decode_pcx(invalid)


if __name__ == "__main__":
    unittest.main()
