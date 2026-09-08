# Спецификация: Однократное приветственное окно подписки на канал AyuGram Fork и полная локализация

## 1. Контекст и Цель
При первом входе пользователя в аккаунт в AyuGram Fork необходимо предложить подписаться на официальный Telegram-канал проекта (`https://t.me/ayufork`).
Окно должно:
- Отображаться строго однократно за всё время использования клиента (сохраняться в персистентных настройках `AyuSettings`).
- Никогда не появляться на экранах входа, авторизации, ввода телефона или регистрации — только после успешного входа в аккаунт, когда отображается основное окно чатов.
- Содержать кнопки «Подписаться» (переход на `https://t.me/ayufork`) и «Позже» (закрытие окна).
- Быть переведено на все поддерживаемые языки в репозитории `Languages` (24 языка), а также включать недостающие переводы для настройки отображения аватарок заблокировавших пользователей (`LoadBlockedAvatars`).

---

## 2. Архитектура решения

### 2.1. Хранение состояния (`AyuSettings`)
* **Флаг:** `_channelPromoShown` (`rpl::variable<bool>`, дефолт: `false`).
* **Методы:**
  * `[[nodiscard]] bool channelPromoShown() const`
  * `void setChannelPromoShown(bool val)`
  * `[[nodiscard]] rpl::producer<bool> channelPromoShownValue() const`
  * `[[nodiscard]] rpl::producer<bool> channelPromoShownChanges() const`
* **Сериализация:** сохраняется в `settings.json` через функции `to_json` и `from_json`.
* **Гарантия однократности:** как только окно собирается отобразиться, флаг немедленно переводится в `true` и вызывается `save()`.

### 2.2. Точка интеграции в жизненный цикл окна
* **Файл:** `Telegram/SourceFiles/window/window_session_controller.cpp`.
* **Метод:** В конструкторе `SessionController::SessionController` в блоке инициализации главного окна `if (_isPrimary)`.
* **Логика:**
  ```cpp
  if (_isPrimary) {
      crl::on_main(base::make_weak(this), [=] {
          checkChannelPromo();
      });
  }
  ```
  `checkChannelPromo()`:
  1. Проверяет `if (AyuSettings::getInstance().channelPromoShown()) return;`.
  2. Немедленно вызывает `AyuSettings::getInstance().setChannelPromoShown(true);`.
  3. Создает модальное окно `Ui::GenericBox` через `_window->show(Box([=](not_null<Ui::GenericBox*> box) { ... }))`.
  4. Задает заголовок `tr::ayu_ChannelPromoTitle()`.
  5. Добавляет текст `tr::ayu_ChannelPromoText()`.
  6. Добавляет кнопку `tr::ayu_ChannelPromoSubscribe()`, вызывающую `UrlClickHandler::Open(u"https://t.me/ayufork"_q); box->closeBox();`.
  7. Добавляет кнопку `tr::ayu_ChannelPromoLater()`, вызывающую `box->closeBox();`.
  8. Устанавливает `box->setCloseByOutsideClick(true);`.

### 2.3. Локализация (24 языка в `Languages` и `lang.strings`)
* **Новые ключи для промо:**
  - `ChannelPromoTitle`: «AyuGram Fork»
  - `ChannelPromoText`: «Подпишитесь на наш официальный Telegram-канал, чтобы быть в курсе новостей и обновлений!»
  - `ChannelPromoSubscribe`: «Подписаться»
  - `ChannelPromoLater`: «Позже»
* **Ключи для аватарок заблокированных (перевод на все языки):**
  - `LoadBlockedAvatars`: «Загружать аватарки через t.me»
  - `LoadBlockedAvatarsDescription`: «Позволяет видеть аватарки пользователей, которые вас заблокировали (при наличии у них юзернейма).»
* **Охват языков:**
  Все 24 каталога в `Languages/values/langs/`:
  `ar`, `be`, `de`, `el`, `en`, `eo`, `es`, `fa`, `fi`, `fr`, `he`, `it`, `lv`, `os`, `pl`, `pt`, `ro`, `ru`, `tr`, `uk`, `vi`, `zh`, `zh-hans`, `zh-hant`.
* **Файлы десктопа:**
  `Telegram/Resources/langs/lang.strings` — добавление определений:
  - `"ayu_ChannelPromoTitle" = "AyuGram Fork";`
  - `"ayu_ChannelPromoText" = "Subscribe to our official Telegram channel for updates and announcements!";`
  - `"ayu_ChannelPromoSubscribe" = "Subscribe";`
  - `"ayu_ChannelPromoLater" = "Later";`

---

## 3. Обработка крайних случаев
* **Несколько аккаунтов:** Настройка хранится на уровне приложения в `AyuSettings` (не внутри отдельного `Main::Session`), поэтому при подключении второго/третьего аккаунта окно повторно не показывается.
* **Закрытие приложения или сбой:** Флаг `channelPromoShown` записывается в `settings.json` до или в момент открытия окна, предотвращая повторный показ при рестарте.
* **Экран логина:** `SessionController` физически не создается, пока пользователь не пройдет авторизацию (вход обслуживается другими контроллерами — `IntroWidget`, `Window::Controller`), поэтому показ на экране логина исключен.
