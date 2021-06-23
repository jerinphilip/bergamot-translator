import yaml
import os
import argparse

import pybergamot
from pybergamot import Service, Response, ResponseOptions

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--path', help="Path to en->de bundle", required=True)
    parser.add_argument('--input', help='Path to input', required=True)
    args = parser.parse_args()
    BERGAMOT_ARCHIVE = args.path

    config = {
       "models": [os.path.join(BERGAMOT_ARCHIVE, "model.intgemm.alphas.bin")],
       "shortlist": [os.path.join(BERGAMOT_ARCHIVE, "lex.s2t.bin"), True, 50, 50],
       "vocabs": [
           os.path.join(BERGAMOT_ARCHIVE, "vocab.deen.spm"),
           os.path.join(BERGAMOT_ARCHIVE, "vocab.deen.spm"),
           ],
       "ssplit-prefix-file": os.path.join(BERGAMOT_ARCHIVE, "nonbreaking_prefix.en"),
       "max-length-break": 128,
       "mini-batch-words": 1024,
       "workspace": 128,
       "skip-cost": True,
       "cpu-threads": 2,
       "quiet": True,
       "quiet-translation": True,
       "gemm-precision": "int8shiftAlphaAll",
       "alignment": True,
       "allow-unk": True
    }

    configStr = yaml.dump(config, sort_keys=False)

    service = Service(configStr)
    options = ResponseOptions();
    options.alignment = True
    options.qualityScores = True
    options.alignmentThreshold = 0.2

    with open(args.input) as fp:
        for idx, line in enumerate(fp, 1):
            line = line.strip()
            response = service.translate(line, options)
            print("--- Line ", idx)
            print(">", response.source.text)
            print("<", response.target.text)
            for s, alignment in enumerate(response.alignments):
                for point in alignment:
                    srange = response.source.word(s, point.src)
                    trange = response.target.word(s, point.tgt)
                    source_word = response.source.text[srange.begin:srange.end]
                    target_word = response.target.text[trange.begin:trange.end]
                    print(f"string: {source_word} -> {target_word}; ByteRanges: {srange} -> {trange}; Indices = {point}")



