#!/usr/bin/env python3
# rilevatore di flicker: per ogni gruppo di 8 frame consecutivi, pixel uguale a t-2 e diverso da t-1 in >=5 casi su 6
import sys, glob, os, numpy as np
from PIL import Image
prefix = sys.argv[1]
files = sorted(glob.glob(prefix + '_*_n.png'))
frames = {int(os.path.basename(f).split('_')[-2]): f for f in files}
starts = sorted(f for f in frames if f - 1 not in frames)
tot = []
for s in starts:
    seq = [np.array(Image.open(frames[s + i]).convert('RGB')).astype(np.int32) for i in range(8) if s + i in frames]
    if len(seq) < 8: continue
    cnt = np.zeros(seq[0].shape[:2], np.int32)
    for t in range(2, 8):
        eq2 = (seq[t] == seq[t-2]).all(axis=2)
        ne1 = (seq[t] != seq[t-1]).any(axis=2)
        cnt += (eq2 & ne1)
    mask = cnt >= 5
    n = int(mask.sum())
    tot.append((s, n))
    if n > 30 and len(sys.argv) > 2:
        out = np.concatenate([seq[0], seq[1], np.where(mask[..., None], np.array([255, 0, 255]), seq[0] // 3)], axis=1).astype(np.uint8)
        Image.fromarray(out).save('%s_%06d.png' % (sys.argv[2], s))
for s, n in tot: print(s, n)
