// All variables specific to translation service
var translationService = undefined;

// Model registry
let modelRegistry = undefined;

// A map of language-pair to TranslationModel object
var languagePairToTranslationModels = new Map();

const BERGAMOT_TRANSLATOR_MODULE = "bergamot-translator-worker.js";
const MODEL_REGISTRY = "../models/registry.json";
const MODEL_ROOT_URL = "../models/";
const PIVOT_LANGUAGE = 'en';

// Information corresponding to each file type
const fileInfo = [
  {"type": "model", "alignment": 256},
  {"type": "lex", "alignment": 64},
  {"type": "vocab", "alignment": 64},
  {"type": "qualityModel", "alignment": 64}
];

const start = Date.now();
let moduleLoadStart;
var Module = {
  preRun: [function() {
    log(`Time until Module.preRun: ${(Date.now() - start) / 1000} secs`);
    moduleLoadStart = Date.now();
  }],
  onRuntimeInitialized: async function() {
    log(`Wasm Runtime initialized Successfully (preRun -> onRuntimeInitialized) in ${(Date.now() - moduleLoadStart) / 1000} secs`);
    const response = await fetch(MODEL_REGISTRY);
    modelRegistry = await response.json();
    postMessage([`import_reply`, modelRegistry]);
  }
};

const log = (message) => {
  console.debug(message);
}

const makeResponse = (response) => {
  alignments = [];
  for (var i = 0; i < response.alignments.size(); i++) {
    alignment = [];
    for (var s = 0; s < response.alignments.get(i).size(); s++) {
      distribution = [];
      for (var t = 0; t < response.alignments.get(i).get(s).size(); t++) {
        distribution.push(response.alignments.get(i).get(s).get(t));
      }
      alignment.push(distribution);
    }
    alignments.push(alignment);
  }

  sourceTokens = [];
  for (var i = 0; i < response.alignments.size(); i++) {
    var result = [];
    var tokens = response.getSourceTokens(i);
    for (var s = 0; s < tokens.size(); s++) {
      result.push(tokens.get(s));
    }
    sourceTokens.push(result);
  }

  targetTokens = [];
  for (var i = 0; i < response.alignments.size(); i++) {
    var result = [];
    var tokens = response.getTargetTokens(i);
    for (var s = 0; s < tokens.size(); s++) {
      result.push(tokens.get(s));
    }
    targetTokens.push(result);
  }

  js_response = {
    'source': response.getOriginalText(),
    'target': response.getTranslatedText(),
    'alignments': alignments,
    'sourceTokens': sourceTokens,
    'targetTokens': targetTokens
  };
  log(js_response);
  return js_response;
}

onmessage = async function(e) {
  const command = e.data[0];
  log(`Message '${command}' received from main script`);
  let result = "";
  if (command === 'import') {
      importScripts(BERGAMOT_TRANSLATOR_MODULE);
  } else if (command === 'load_model') {
      let start = Date.now();
      let from = e.data[1];
      let to = e.data[2];
      try {
        await constructTranslationService();
        await constructTranslationModel(from, to);
        log(`Model '${from}${to}' successfully constructed. Time taken: ${(Date.now() - start) / 1000} secs`);
        result = "Model successfully loaded";
      } catch (error) {
        log(`Model '${from}${to}' construction failed: '${error.message}'`);
        result = "Model loading failed";
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([`${command}_reply`, result]);
  } else if (command === 'translate') {
      const from = e.data[1];
      const to = e.data[2];
      const input = e.data[3];
      const translateOptions = e.data[4];
      let inputWordCount = 0;
      let inputBlockElements = 0;
      input.forEach(sentence => {
        inputWordCount += sentence.trim().split(" ").filter(word => word.trim() !== "").length;
        inputBlockElements++;
      })
      let start = Date.now();
      try {
        log(`Blocks to translate: ${inputBlockElements}`);
        result = translate(from, to, input, translateOptions);
        const secs = (Date.now() - start) / 1000;
        log(`Translation '${from}${to}' Successful. Speed: ${Math.round(inputWordCount / secs)} WPS (${inputWordCount} words in ${secs} secs)`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([`${command}_reply`, result]);
  }
}

// Instantiates the Translation Service
const constructTranslationService = async () => {
  if (!translationService) {
    var translationServiceConfig = {cacheSize: 20000};
    log(`Creating Translation Service with config: ${translationServiceConfig}`);
    translationService = new Module.BlockingService(translationServiceConfig);
    log(`Translation Service created successfully`);
  }
}

// Constructs translation model(s) for the source and target language pair (using
// pivoting if required).
const constructTranslationModel = async (from, to) => {
  // Delete all previously constructed translation models and clear the map
  languagePairToTranslationModels.forEach((value, key) => {
    log(`Destructing model '${key}'`);
    value.delete();
  });
  languagePairToTranslationModels.clear();

  if (_isPivotingRequired(from, to)) {
    // Pivoting requires 2 translation models
    const languagePairSrcToPivot = _getLanguagePair(from, PIVOT_LANGUAGE);
    const languagePairPivotToTarget = _getLanguagePair(PIVOT_LANGUAGE, to);
    await Promise.all([_constructTranslationModelHelper(languagePairSrcToPivot),
                      _constructTranslationModelHelper(languagePairPivotToTarget)]);
  }
  else {
    // Non-pivoting case requires only 1 translation model
    await _constructTranslationModelHelper(_getLanguagePair(from, to));
  }
}

// Translates text from source language to target language (via pivoting if necessary).
const translate = (from, to, input, translateOptions) => {
  let vectorResponseOptions, vectorSourceText, vectorResponse;
  try {
    // Prepare the arguments (vectorResponseOptions and vectorSourceText (vector<string>)) of Translation API and call it.
    // Result is a vector<Response> where each of its item corresponds to one item of vectorSourceText in the same order.
    vectorResponseOptions = _prepareResponseOptions(translateOptions);
    vectorSourceText = _prepareSourceText(input);

    if (_isPivotingRequired(from, to)) {
      // Translate via pivoting
      const translationModelSrcToPivot = _getLoadedTranslationModel(from, PIVOT_LANGUAGE);
      const translationModelPivotToTarget = _getLoadedTranslationModel(PIVOT_LANGUAGE, to);
      vectorResponse = translationService.translateViaPivoting(translationModelSrcToPivot,
                                                              translationModelPivotToTarget,
                                                              vectorSourceText,
                                                              vectorResponseOptions);
    }
    else {
      // Translate without pivoting
      const translationModel = _getLoadedTranslationModel(from, to);
      vectorResponse = translationService.translate(translationModel, vectorSourceText, vectorResponseOptions);
    }

    responses = []
    for(var i = 0; i < vectorResponse.size(); i++){
        responses.push(makeResponse(vectorResponse.get(i)));
    }

    return responses;
  } finally {
    // Necessary clean up
    if (vectorSourceText != null) vectorSourceText.delete();
    if (vectorResponseOptions != null) vectorResponseOptions.delete();
    if (vectorResponse != null) vectorResponse.delete();
  }
}

// Downloads file from a url and returns the array buffer
const _downloadAsArrayBuffer = async(url) => {
  const response = await fetch(url);
  if (!response.ok) {
    throw Error(`Downloading ${url} failed: HTTP ${response.status} - ${response.statusText}`);
  }
  return response.arrayBuffer();
}

// Constructs and initializes the AlignedMemory from the array buffer and alignment size
const _prepareAlignedMemoryFromBuffer = async (buffer, alignmentSize) => {
  var byteArray = new Int8Array(buffer);
  var alignedMemory = new Module.AlignedMemory(byteArray.byteLength, alignmentSize);
  const alignedByteArrayView = alignedMemory.getByteArrayView();
  alignedByteArrayView.set(byteArray);
  return alignedMemory;
}

async function prepareAlignedMemory(file, languagePair) {
  const fileName = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair][file.type].name}`;
  const buffer = await _downloadAsArrayBuffer(fileName);
  const alignedMemory = await _prepareAlignedMemoryFromBuffer(buffer, file.alignment);
  log(`"${file.type}" aligned memory prepared. Size:${alignedMemory.size()} bytes, alignment:${file.alignment}`);
  return alignedMemory;
}

const _constructTranslationModelHelper = async (languagePair) => {
  log(`Constructing translation model ${languagePair}`);

  /*Set the Model Configuration as YAML formatted string.
    For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
    Vocab files are re-used in both translation directions.
    DO NOT CHANGE THE SPACES BETWEEN EACH ENTRY OF CONFIG
  */
  const modelConfig = `beam-size: 1
normalize: 1.0
word-penalty: 0
max-length-break: 128
mini-batch-words: 1024
workspace: 128
max-length-factor: 2.0
skip-cost: false
cpu-threads: 0
quiet: true
quiet-translation: true
gemm-precision: int8shiftAlphaAll
alignment: soft
`;

  const promises = [];
  fileInfo.filter(file => modelRegistry[languagePair].hasOwnProperty(file.type))
  .map((file) => {
      promises.push(prepareAlignedMemory(file, languagePair));
  });

  const alignedMemories = await Promise.all(promises);

  log(`Translation Model config: ${modelConfig}`);
  log(`Aligned memory sizes: Model:${alignedMemories[0].size()} Shortlist:${alignedMemories[1].size()} Vocab:${alignedMemories[2].size()}`);
  const alignedVocabMemoryList = new Module.AlignedMemoryList();
  alignedVocabMemoryList.push_back(alignedMemories[2]);
  let translationModel;
  if (alignedMemories.length === fileInfo.length) {
    log(`QE:${alignedMemories[3].size()}`);
    translationModel = new Module.TranslationModel(modelConfig, alignedMemories[0], alignedMemories[1], alignedVocabMemoryList, alignedMemories[3]);
  }
  else {
    translationModel = new Module.TranslationModel(modelConfig, alignedMemories[0], alignedMemories[1], alignedVocabMemoryList, null);
  }
  languagePairToTranslationModels.set(languagePair, translationModel);
}

const _isPivotingRequired = (from, to) => {
  return (from !== PIVOT_LANGUAGE) && (to !== PIVOT_LANGUAGE);
}

const _getLanguagePair = (srcLang, tgtLang) => {
  return `${srcLang}${tgtLang}`;
}

const _getLoadedTranslationModel = (srcLang, tgtLang) => {
  const languagePair = _getLanguagePair(srcLang, tgtLang);
  if (!languagePairToTranslationModels.has(languagePair)) {
    throw Error(`Translation model '${languagePair}' not loaded`);
  }
  return languagePairToTranslationModels.get(languagePair);
}

const _prepareResponseOptions = (translateOptions) => {
  let vectorResponseOptions = new Module.VectorResponseOptions;
  translateOptions.forEach(translateOption => {
    vectorResponseOptions.push_back({
      qualityScores: translateOption["isQualityScores"],
      alignment: true,
      html: translateOption["isHtml"]
    });
  });
  if (vectorResponseOptions.size() == 0) {
    vectorResponseOptions.delete();
    throw Error(`No Translation Options provided`);
  }
  return vectorResponseOptions;
}

const _prepareSourceText = (input) => {
  let vectorSourceText = new Module.VectorString;
  input.forEach(paragraph => {
    // prevent empty paragraph - it breaks the translation
    if (paragraph.trim() === "") {
      return;
    }
    vectorSourceText.push_back(paragraph.trim())
  })
  if (vectorSourceText.size() == 0) {
    vectorSourceText.delete();
    throw Error(`No text provided to translate`);
  }
  return vectorSourceText;
}

