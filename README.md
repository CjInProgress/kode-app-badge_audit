# Badge Audit

A Kode Dot app that reads an access badge and tells you how secure it is. Hold a badge to the Dot and the screen shows the badge's family, its UID and a plain verdict, such as "Cloneable: no cryptography", with a one-line tip on what to do about it.

Badge Audit is a defensive tool. It reads a badge's identity and assesses it. It never copies, writes or emulates a badge, and it saves nothing: the history lives in RAM and is gone when the app ends.

## Using it

- Scanning starts when the app opens. Each scan looks for a 13.56 MHz badge (NFC) for 0.6 s, then for a 125 kHz badge (RFID) for 0.4 s, and repeats.
- A new badge goes on the card at once: a lamp and the verdict in its colour, the model and band, the UID and the tip.
- A badge left on the Dot is counted, not added again. The footer says "1 of 3, read 4x".
- **UP** shows an older badge from the history and **DOWN** a newer one. The history keeps the last 8 badges of the session.
- **TOP**, or a tap anywhere on the glass, pauses and resumes scanning. Pausing turns the readers' fields off between scans, which saves battery.
- Holding **BOT** returns to kodeOS, as in every app.
- Every new badge is also logged on the USB console: `badge_audit.ao: Mifare Classic 1K (13.56 MHz), UID 04:A2:3B:91: Weak: broken Crypto-1`.
- If a reader fails (for example "NFC is busy, or no reader is fitted"), the status line shows the reader's error in yellow and scanning rests for 2 s before it tries again.

## The security table

The verdict comes from the band the badge answered on and the model name the SDK's reader gives. The table is in `main/badge_class.c`. The first matching row wins, and each band ends in a catch-all row.

| Band | Reader's model name | Verdict | Colour | Why | Tip on screen |
|---|---|---|---|---|---|
| 125 kHz | `EM4100` | Cloneable: no cryptography | red | A read-only ID sent in clear to any reader that powers it. A copy takes seconds and a cheap writer. | It sends its ID in clear to any reader. Replace it with DESFire EV2/EV3. |
| 125 kHz | anything else | Cloneable: no cryptography | red | 125 kHz proximity badges (EM, HID Prox, Indala) have no cryptography. | 125 kHz badges (EM, HID Prox) carry no crypto. Plan a move to 13.56 MHz. |
| 13.56 MHz | `Mifare Classic`, `Mifare Classic 1K`, `4K`, `1K (TNP3xxx)` | Weak: broken Crypto-1 | yellow | Crypto-1 was broken in 2008. Keys are recovered with free tools in minutes, after which the card can be copied whole. | Its keys fall to free tools in minutes. Move to DESFire EV2/EV3. |
| 13.56 MHz | `Mifare Mini` | Weak: broken Crypto-1 | yellow | A small Mifare Classic with the same cipher. | A small Mifare Classic, with the same broken cipher. Replace it. |
| 13.56 MHz | `Ultralight / NTAG (Type-2)` | No protection | red | Made for tickets and NFC tags. Its memory and UID are readable by anyone, and its UID is easy to emulate. | Made for posters and tickets, not doors: its ID is trivial to copy. |
| 13.56 MHz | `Type-4 (ISO-DEP / DESFire)` | Strong if the reader checks keys | green | DESFire EV2/EV3 use AES with mutual authentication, which has no practical attack. That only helps if the reader authenticates. A reader that checks only the UID makes any card as easy to copy as a 125 kHz badge. | Safe only if the reader runs AES checks; one that trusts the UID is not. |
| 13.56 MHz | anything else (`NFC-A`) | Unknown family | grey, lamp off | The reader could not name it. | The reader could not name it. Check its markings before you trust it. |

A 4-byte UID that starts with `08` is shown as "(random)". ISO/IEC 14443-3 uses it for a random UID, which a DESFire card sends in privacy mode. Such a card changes its UID at every read, so the history matches it by model instead of by UID.

### What the table cannot tell

- **DESFire EV1, EV2 and EV3 look the same.** The SDK names a card from its SAK byte. Every ISO-DEP card, including DESFire EV1 (whose crypto has known weaknesses), Mifare Plus in SL3 and payment cards, comes back as `Type-4 (ISO-DEP / DESFire)`. Telling them apart would mean sending GetVersion over ISO-DEP, which `kode.h` does not offer yet.
- **Mifare Plus in SL1 looks like a Mifare Classic,** and that rating is right: in SL1 it uses Crypto-1.
- **Ultralight C, NTAG 21x passwords and originality signatures are not checked.** They are all `Ultralight / NTAG (Type-2)` to the reader.
- **Only EM4100 is decoded at 125 kHz.** The SDK's RFID front end reads EM4100 only. A HID Prox or Indala badge is not seen at all, so a blank screen with a 125 kHz badge does not mean the badge is safe.
- **The verdict rates the badge, not the door.** Most access systems read only the UID, and then every badge in this table is cloneable. Ask whoever runs the system whether the readers authenticate the card.

## Building

The app pins kode-sdk **v0.1.1** (`main/idf_component.yml`) and builds with ESP-IDF v6.1.

```bash
idf.py build
```

Check the internal-RAM budget (catalog rule B8) with the SDK's script:

```bash
<kode-sdk>/tools/check_ram.sh build/badge_audit.elf ram_budget.txt
```

`ram_budget.txt` is the template's v0.1.1 budget with two app-specific changes. `lv_global` (LVGL's state, 17,408 bytes, the SDK's own line) is added because the app draws through `kscreen`. The app's own `s_stack` was already listed. The history (8 entries, about 600 bytes) and the scan buffer are small statics. The worker's 8 KB stack is in PSRAM. On the last build, static internal RAM was 265,524 of 368,520 bytes.

`sdkconfig.defaults` differs from the template in two places:
- It turns on `CONFIG_KODEOS_NFC=y`, which `kode_nfc_*()` needs in order to link.
- It drops `CONFIG_ESP_HOSTED_ENABLED=n`. From SDK v0.1.1 on, that line comes from kbsp's `sdkconfig.board`, and repeating it is a B5 reject.

## Installing on the Dot

With kodeOS on the board, write the image to the app mailbox and let kodeOS adopt it:

```bash
python -m esptool --chip esp32p4 -p /dev/cu.usbmodem101 write-flash 0x1C00000 build/badge_audit.bin
```

## How it is built

| File | What it does |
|---|---|
| `main/app_main.c` | `kode_init()`, the manifest (`"Badge Audit"`, `tools,wireless`), and starts the Active Object. |
| `main/app_ao.c` | The Active Object, at `KODE_PRIO_USER_GUI`. It drives LVGL through `kscreen` and never blocks. Scans run as jobs on its `kode_worker`, because `kode_nfc_read_id()` and `kode_rfid_read_id()` block. Each job's result comes back in one done event, and the Active Object resubmits the next scan. |
| `main/app_ui.c` | The screen, built with kui's faces, tokens, flex boxes and lamp. Only the Active Object calls it. At startup it runs `kui_panel_check()` and logs anything that falls in the glass's corners. |
| `main/badge_scan.c` | The scan job, run on the worker: `kode_nfc_read_id()` for 0.6 s, then `kode_rfid_read_id()` for 0.4 s. It copies the reader's `kode_error()`, which belongs to the worker's task, unless the error only says no badge came. |
| `main/badge_class.c` | The security table and the random-UID check. Plain C with no SDK calls, so it can be tested on a host. |
| `main/badge_log.c` | The session's history: a ring of 8 entries in RAM. |

## Status

Built and checked with `check_ram.sh`, but **not run on a Kode Dot yet**. The author's board has not arrived. Before this goes to the catalog:

- Run it on a board, try one badge of each family, and check the layout. `kui_panel_check()` logs any object in the corners at startup. The longest verdict wraps to two lines.
- Replace `appdata/screenshots/1.png`, which is still the template's placeholder, with screenshots taken with `kcli.py screenshot` (rule D9).
- Run `test component=mem` after a few minutes of scanning (rules B8 and C9).

## Licence

GPL-3.0-or-later, like the SDK it links, with the additional permission for RFAL: see `LICENSE` and `NOTICE.md`.
