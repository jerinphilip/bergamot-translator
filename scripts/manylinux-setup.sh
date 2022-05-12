#!/bin/bash

set -eo pipefail
set -x

yum install python*-devel -y

yum-config-manager -y --add-repo https://yum.repos.intel.com/mkl/setup/intel-mkl.repo
yum install -y intel-mkl

# Temporary: Add an exception for github.workspace
# TODO(jerinphilip): Remove
git config --global --add safe.directory /github/workspace
git config --global --add safe.directory /github/workspace/3rd_party/marian-dev
git config --global --add safe.directory /github/workspace/3rd_party/ssplit-cpp
