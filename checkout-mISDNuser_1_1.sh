#!/bin/bash

#checks out the mISDNuser_1_1 branch

git-checkout -b mISDNuser_1_1 origin/mISDNuser_1_1
git-config branch.mISDNuser_1_1.remote origin
git-config branch.mISDNuser_1_1.merge refs/heads/mISDNuser_1_1

