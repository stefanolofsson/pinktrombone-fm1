#!/usr/bin/env python3
"""The YouTube thumbnail (1280 x 720) from a frame of the vowels scene: python3 thumbnail.py"""
import glob
from PIL import Image, ImageDraw, ImageFont
fr = sorted(glob.glob('out/vowels/frames/*.ppm'))
im = Image.open(fr[int((2.2 + 8.5) * 25)])
lcd = im.crop((370, 40, 850, 522)).resize((600, 602), Image.LANCZOS)
W, H = 1280, 720
th = Image.new('RGB', (W, H), (22, 9, 18))
d = ImageDraw.Draw(th)
d.rounded_rectangle((36, 56, 36 + 608, 56 + 610), 22, fill=(255, 120, 180))
th.paste(lcd, (40, 60))
F = lambda sz, i=0: ImageFont.truetype("/System/Library/Fonts/Avenir Next.ttc", sz, index=i)
d.text((690, 110), "PINK", font=F(150, 0), fill=(255, 120, 180))
d.text((690, 270), "TROMBONE", font=F(98, 0), fill=(255, 120, 180))
d.text((694, 410), "a singing voice synth", font=F(44, 5), fill=(235, 220, 228))
d.text((694, 466), "for the M-VAVE FM-1", font=F(44, 5), fill=(235, 220, 228))
d.rounded_rectangle((694, 570, 1000, 636), 16, fill=(255, 255, 255))
d.text((722, 578), "FIRMWARE", font=F(44, 0), fill=(22, 9, 18))
th.save('out/thumbnail.png')
