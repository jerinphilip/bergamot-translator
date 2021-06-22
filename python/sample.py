import yaml
import os
import argparse

import pybergamot
from pybergamot import Service, Response, ResponseOptions

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--path', help="Path to en->de bundle", required=True)
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
    }

    configStr = yaml.dump(config, sort_keys=False)

    service = Service(configStr)
    options = ResponseOptions();
    options.alignment = True

    response = service.translate("Hello World, what can I interest you with?", options)
    print(response.source.text)
    print(response.target.text)




