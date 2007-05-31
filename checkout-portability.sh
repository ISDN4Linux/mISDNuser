#!/bin/bash

#checks out the portability branch

git-checkout -b portability origin/portability
git-config branch.portability.remote origin
git-config branch.portability.merge refs/heads/portability

