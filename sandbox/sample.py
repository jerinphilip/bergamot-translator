
import yaml
import os
import argparse
import sys
import pandas as pd


sys.path.insert(0, "../build")
import pybergamot
from pybergamot import Service, Response, ResponseOptions

def build_config(BERGAMOT_ARCHIVE):
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
       "cpu-threads": 40,
       "quiet": True,
       "quiet-translation": True,
       "gemm-precision": "int8shiftAlphaAll",
       "alignment": True,
       "allow-unk": True,
       "log": "marian-log.txt",
       "log-level": "debug"
    }
    return config

def build_service(bergamot_path):
    config = build_config(bergamot_path)
    configStr = yaml.dump(config, sort_keys=False)
    service = Service(configStr)
    return service


ENDE_BUNDLE = '../bergamot-translator-tests/models/deen/ende.student.tiny.for.regression.tests/'
# For a pair of (src, tgt): Accumulate what the unknowns point to.
# Do we do hard alignment?
service = build_service(ENDE_BUNDLE)

# We work with these options, for now.
options = ResponseOptions();
options.alignment = True
options.qualityScores = True
options.alignmentThreshold = 0.2

from collections import defaultdict

def acc(service, options, line):
    acc_d = defaultdict(list)
    response = service.translate(line, options)
    # print("--- Line ")
    # print(">", response.source.text)
    # print("<", response.target.text)


    for sentenceIdx, alignment in enumerate(response.alignments):
        for point in alignment:
            sbr = response.source.word(sentenceIdx, point.src)
            tbr = response.target.word(sentenceIdx, point.tgt)
            if response.source.isUnknown(sentenceIdx, point.src):
                acc_d[sbr].append(tbr)

    return acc_d

data = pd.read_csv('MTNT/train/train.en-fr.tsv', sep='\t', error_bad_lines=False, names=['No', 'src', 'tgt'])
global_d = defaultdict(list)
size = data.size

source_lines = data['src'].values
sorted_source_lines = sorted(source_lines, key=len)
content = '\n'.join(sorted_source_lines[int(sys.argv[1]):int(sys.argv[2])])
with open(sys.argv[3], 'w+') as fp:
    fp.write(content)

d = acc(service, options, content)
for k, v in d.items():
    global_d[k].extend(v)
    from collections import Counter
for k in global_d:
    print('Source[{}]'.format(k), 'Target[{}]'.format(Counter(global_d[k])))


