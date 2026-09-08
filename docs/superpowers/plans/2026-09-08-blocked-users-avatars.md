# Blocked Users Avatars Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow AyuGram users to view profile pictures of users who blocked them by resolving public preview avatars from `t.me/<username>` in the background.

**Architecture:** A lightweight queue-based resolver `AyuAvatarResolver` runs background HTTP requests using `QNetworkAccessManager` to parse `t.me/<username>`, extracts the public CDN photo URL, downloads the image, caches it to disk, and applies it to `UserData` via `InMemoryLocation`. Configurable via AyuGram settings.

**Tech Stack:** C++20, Qt 6 (`QNetworkAccessManager`, `QImage`, `QRegularExpression`), Telegram Desktop data structures (`UserData`, `ImageLocation`, `InMemoryLocation`).

## Global Constraints
- Target platform: Windows / Linux / macOS (AyuGram Desktop codebase).
- Respect `AGENTS.md`: Avoid heavy Release builds.
- Respect network limits: concurrency limit (max 2 parallel requests), negative caching (2h TTL), 5-second timeouts.
- Preserve existing comments and style formatting in modified files.

---

### Task 1: Add localization keys to Languages and lang.strings

**Files:**
- Modify: `c:/Users/aver/Desktop/ayu/Languages/values/langs/en/Shared.json`
- Modify: `c:/Users/aver/Desktop/ayu/Languages/values/langs/ru/Shared.json`
- Modify: `c:/Users/aver/Desktop/ayu/AyuGramDesktop-dev/Telegram/Resources/langs/lang.strings:8810`

**Interfaces:**
- Produces: `tr::ayu_LoadBlockedAvatars()`, `tr::ayu_LoadBlockedAvatarsDescription()`

- [ ] **Step 1: Add English translation strings to `Languages/.../en/Shared.json`**
Add:
```json
  "LoadBlockedAvatars": "Load Avatars via t.me",
  "LoadBlockedAvatarsDescription": "Allows you to see profile pictures of users who blocked you, if they have a public username."
```

- [ ] **Step 2: Add Russian translation strings to `Languages/.../ru/Shared.json`**
Add:
```json
  "LoadBlockedAvatars": "Загружать аватарки через t.me",
  "LoadBlockedAvatarsDescription": "Позволяет видеть аватарки пользователей, которые вас заблокировали (при наличии у них юзернейма)."
```

- [ ] **Step 3: Add generated keys to `Telegram/Resources/langs/lang.strings`**
Append to `Telegram/Resources/langs/lang.strings`:
```
"ayu_LoadBlockedAvatars" = "Load Avatars via t.me";
"ayu_LoadBlockedAvatarsDescription" = "Allows you to see profile pictures of users who blocked you, if they have a public username.";
```

- [ ] **Step 4: Commit**
```bash
git add Telegram/Resources/langs/lang.strings
git commit -m "i18n: add loadBlockedAvatars strings"
```

---

### Task 2: Add `loadBlockedAvatars` setting to `AyuSettings`

**Files:**
- Modify: `Telegram/SourceFiles/ayu/ayu_settings.h`
- Modify: `Telegram/SourceFiles/ayu/ayu_settings.cpp`

**Interfaces:**
- Produces:
  - `bool AyuSettings::loadBlockedAvatars() const`
  - `void AyuSettings::setLoadBlockedAvatars(bool value)`

- [ ] **Step 1: Declare property in `ayu_settings.h`**
In `ayu_settings.h`:
```cpp
[[nodiscard]] bool loadBlockedAvatars() const {
    return _loadBlockedAvatars;
}
void setLoadBlockedAvatars(bool value);
```
Add field:
```cpp
bool _loadBlockedAvatars = true;
```

- [ ] **Step 2: Implement getter/setter and serialization in `ayu_settings.cpp`**
In `loadSettings()`:
```cpp
_loadBlockedAvatars = obj.value("loadBlockedAvatars").toBool(true);
```
In `saveSettings()`:
```cpp
obj["loadBlockedAvatars"] = _loadBlockedAvatars;
```
Implement setter:
```cpp
void AyuSettings::setLoadBlockedAvatars(bool value) {
    if (_loadBlockedAvatars != value) {
        _loadBlockedAvatars = value;
        save();
    }
}
```

- [ ] **Step 3: Commit**
```bash
git add Telegram/SourceFiles/ayu/ayu_settings.h Telegram/SourceFiles/ayu/ayu_settings.cpp
git commit -m "feat(ayu): add loadBlockedAvatars setting"
```

---

### Task 3: Implement `AyuAvatarResolver`

**Files:**
- Create: `Telegram/SourceFiles/ayu/features/avatars/ayu_avatar_resolver.h`
- Create: `Telegram/SourceFiles/ayu/features/avatars/ayu_avatar_resolver.cpp`

**Interfaces:**
- Produces:
  - `class AyuAvatarResolver`
  - `static AyuAvatarResolver &AyuAvatarResolver::Instance()`
  - `void AyuAvatarResolver::resolve(not_null<UserData*> user)`

- [ ] **Step 1: Write header `ayu_avatar_resolver.h`**
Define `AyuAvatarResolver`:
- Queue for pending users
- In-progress set `base::flat_set<QString>`
- Negative cache `base::flat_map<QString, crl::time>` with 2-hour TTL
- Concurrency limit (max 2 requests)
- Methods to check disk cache and dispatch HTTP GET requests for HTML page and CDN image.

- [ ] **Step 2: Write implementation `ayu_avatar_resolver.cpp`**
Implement:
1. `resolve(not_null<UserData*> user)`:
   - Check if setting is enabled (`AyuSettings::getInstance().loadBlockedAvatars()`).
   - Check if user already has photo or has empty username.
   - Check negative cache (skip if recently failed within 2 hours).
   - Check disk cache (`caching/avatars/<username>.jpg`): if file exists, read bytes, decode `QImage`, call `applyUserpic(user, image)`, and return.
   - Otherwise, push to queue and trigger `processQueue()`.
2. `processQueue()`:
   - While `_activeRequests < 2` and `!_queue.empty()`:
     - Pop next task, check `_inProgress`.
     - Request `https://t.me/<username>` with browser User-Agent.
     - On HTML response:
       - Match `og:image` or `tgme_page_photo_image` pointing to `telesco.pe` or Telegram CDN.
       - If no avatar found: put in negative cache with current timestamp.
       - If found: request image URL, save to disk cache, call `applyUserpic(user, image)`.
3. `applyUserpic(not_null<UserData*> user, const QImage &image)`:
   - Convert image to JPG bytes via `QBuffer`.
   - Call `user->setUserpic(base::RandomValue<PhotoId>(), ImageLocation({ .data = InMemoryLocation{ .bytes = bytes } }, image.width(), image.height()), false);`
   - Notify `user->session().changes().peerUpdated(user, UpdateFlag::Photo);`

- [ ] **Step 3: Commit**
```bash
git add Telegram/SourceFiles/ayu/features/avatars/
git commit -m "feat(ayu): implement AyuAvatarResolver"
```

---

### Task 4: Hook `AyuAvatarResolver` into UserData and Profile View

**Files:**
- Modify: `Telegram/SourceFiles/data/data_user.cpp`
- Modify: `Telegram/SourceFiles/info/profile/info_profile_widget.cpp`

**Interfaces:**
- Consumes: `AyuAvatarResolver::Instance().resolve(user)`

- [ ] **Step 1: Hook in `UserData::setPhoto`**
In `Telegram/SourceFiles/data/data_user.cpp`:
In the `MTPDuserProfilePhotoEmpty` handler in `setPhoto`:
```cpp
#include "ayu/features/avatars/ayu_avatar_resolver.h"
#include "ayu/ayu_settings.h"

// inside setPhoto lambda for MTPDuserProfilePhotoEmpty:
if (AyuSettings::getInstance().loadBlockedAvatars() && !username().isEmpty()) {
    AyuAvatarResolver::Instance().resolve(this);
}
```

- [ ] **Step 2: Hook in `info_profile_widget.cpp` when opening profile**
In `info_profile_widget.cpp`, when viewing a user profile:
If `user && !user->hasUserpic() && !user->username().isEmpty() && AyuSettings::getInstance().loadBlockedAvatars()`:
Call `AyuAvatarResolver::Instance().resolve(user)`.

- [ ] **Step 3: Commit**
```bash
git add Telegram/SourceFiles/data/data_user.cpp Telegram/SourceFiles/info/profile/info_profile_widget.cpp
git commit -m "feat(ayu): hook avatar resolver into UserData and profile view"
```

---

### Task 5: Add UI toggle in Settings

**Files:**
- Modify: `Telegram/SourceFiles/ayu/ui/settings/settings_general.cpp`

**Interfaces:**
- Consumes: `AyuSettings::loadBlockedAvatars`, `tr::ayu_LoadBlockedAvatars`

- [ ] **Step 1: Add setting toggle in `settings_general.cpp`**
In `BuildQoLToggles`:
```cpp
ayu.addSettingToggle({
    .id = u"ayu/loadBlockedAvatars"_q,
    .title = tr::ayu_LoadBlockedAvatars(),
    .description = tr::ayu_LoadBlockedAvatarsDescription(),
    .getter = &AyuSettings::loadBlockedAvatars,
    .setter = &AyuSettings::setLoadBlockedAvatars,
});
```

- [ ] **Step 2: Commit**
```bash
git add Telegram/SourceFiles/ayu/ui/settings/settings_general.cpp
git commit -m "feat(ayu): add loadBlockedAvatars toggle to settings UI"
```

---

### Task 6: Register files in `Telegram/CMakeLists.txt`

**Files:**
- Modify: `Telegram/CMakeLists.txt`

- [ ] **Step 1: Add source files to `Telegram/CMakeLists.txt`**
Add:
```cmake
        ayu/features/avatars/ayu_avatar_resolver.h
        ayu/features/avatars/ayu_avatar_resolver.cpp
```
under the `ayu/` section.

- [ ] **Step 2: Commit**
```bash
git add Telegram/CMakeLists.txt
git commit -m "build: register ayu_avatar_resolver in CMakeLists.txt"
```
