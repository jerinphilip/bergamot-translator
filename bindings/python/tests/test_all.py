# type: ignore
import pytest
from bergamot import REPOSITORY, Service
from bergamot.utils import toJSON


def test_basic():
    keys = ["browsermt"]
    models = ["de-en-tiny"]
    service = Service(num_workers=1, log_level="critical")
    for repository in keys:
        # models = REPOSITORY.models(repository, filter_downloaded=False)
        for model in models:
            REPOSITORY.download(repository, model)

        for modelId in models:
            configPath = REPOSITORY.modelConfigPath(repository, modelId)
            model = service.modelFromConfigPath(configPath)
            print(repository, modelId)
            source = "1 2 3 4 5 6 7 8 9"
            responses = service.translate(
                model,
                [source],
                alignment=True,
                quality_scores=True,
                html=False,
            )
            for response in responses:
                print(toJSON(response, indent=4))
