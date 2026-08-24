### Task 1: Merge upstream tag `v7.1.1` into `dev` branch

**Files:**
- Modify: git repository state in `AyuGramDesktop-dev`

**Interfaces:**
- Consumes: git remote `tdesktop`, tag `v7.1.1`
- Produces: merged branch with conflicts marked or clean merge commit

- [ ] **Step 1: Fetch and initiate merge of tag `v7.1.1`**
Run: `git merge v7.1.1 --no-commit --no-ff`
- [ ] **Step 2: Inspect conflict list and changed files**
Run: `git status` to identify any conflicting files.
- [ ] **Step 3: Update submodules if necessary**
Run: `git submodule update --init --recursive`
