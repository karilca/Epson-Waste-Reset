# EWR (Epson Waste Reset)
![Platforma](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue)
![C++](https://img.shields.io/badge/language-C++17-orange)
![Licenca](https://img.shields.io/badge/license-Apache_License_2.0-green)

Besplatan, višeplatformski i potpuno open-source C++ alat za resetiranje brojača "Waste Ink Pad" na Epson pisačima.

EWR zaobilazi potrebu za plaćanjem sumnjivih ključeva za resetiranje trećih strana (poput WIC Reset) ili pokretanja zlonamjernih, antivirusno označenih `AdjProg.exe` binarnih datoteka. Dinamičkim generiranjem IEEE 1284.4 hardverskih paketa i korištenjem kontinuirano ažurirane baze podataka, EWR komunicira izravno s matičnom pločom pisača putem USB-a kako bi sigurno nulirao EEPROM waste brojače.

## Značajke
* **Pametni protokolarni mehanizam:** Konstruira točne EEPROM write pakete (`|B`) u hodu na temelju specifičnih modela pisača. Sigurno upravlja IEEE 1284.4 (D4) hardverskim kreditnim sustavom kako bi spriječio prekoračenje međuspremnika i zaključavanja.
* **OTA sinkronizacija baze podataka:** Automatski dohvaća ogromnu, kontinuirano ažuriranu bazu podataka offseta i ključeva pisača pri pokretanju koristeći nativne OS API-je (bez dodatnog opterećenja).
* **Višeplatformska jezgra:**
  * **Windows:** Koristi 100% nativni Win32 `SetupAPI` i robusni asinkroni `OVERLAPPED` I/O za sigurno pražnjenje Windows Print Spooler međuspremnika. Nisu potrebni nikakvi prilagođeni drajveri.
  * **Linux:** Koristi `libusb` za automatsko odvajanje kernel drajvera (CUPS) radi ekskluzivnog, izravnog hardverskog pristupa.
* **Nula hardkodiranih PID-ova:** Automatski skenira USB stablo vašeg OS-a kako bi pronašao priključene Epson pisače.
* **Replay rezervna metoda:** Ako vaš pisač još nije u bazi podataka, EWR može dinamički parsirati i izvršiti sirove Wireshark snimke (automatski uklanjajući USBPcap zaglavlja).

### Preduvjeti (Za izgradnju iz izvornog koda)
* **Windows:** Visual Studio s MSVC C++ alatima za izgradnju.
* **Linux (Arch/Debian):** `cmake`, `gcc`, `pkgconf`, `libusb-1.0-dev` i `libcurl4-openssl-dev`.

## Korištenje

1. Provjerite je li vaš Epson pisač uključen i spojen na računalo putem USB-a.
2. Pokrenite izvršnu datoteku:
   * **Windows:** Dvaput kliknite na `ewr.exe`
   * **Linux:** `sudo ./ewr` *(Izravni USB pristup zahtijeva root prava)*
3. **Napomena:** Pri prvom pokretanju, EWR zahtijeva internetsku vezu za preuzimanje najnovije baze podataka pisača. Nakon toga, radi potpuno offline.
4. Unesite broj koji odgovara vašem pisaču i pritisnite Enter.
5. Pričekajte poruku `SUCCESS`, zatim **isključite pisač i ponovno ga uključite fizičkim gumbom za napajanje** kako biste EEPROM promjene pohranili na matičnu ploču.

## Izgradnja iz izvornog koda

Otvorite terminal u korijenu repozitorija i pokrenite:

```bash
# 1. Generirajte datoteke za izgradnju
cmake -B build

# 2. Kompajlirajte projekt (Release način)
cmake --build build --config Release
```
Kompajlirana izvršna datoteka `(ewr.exe ili ewr)` nalazit će se u direktoriju `Release`.

## 🤝 Doprinos novim modelom pisača (Replay rezervna metoda)

Ako vaš pisač još nije u bazi podataka, možete mu dodati podršku korištenjem naše Replay metode bez pisanja ijedne linije koda!

### Korak 1: Snimite hardverski razgovor
1. Instalirajte [Wireshark](https://www.wireshark.org/) (provjerite da je **USBPcap** instaliran na Windowsima) na vaš VM
2. Spojite pisač na računalo i uključite ga
3. Spojite pisač na VM
4. Otvorite Wireshark i pokrenite snimanje na vašem USB sučelju
5. Otvorite sumnjivi Epson program za podešavanje koji ste pronašli na internetu unutar VM-a (ovo čuva vaše host računalo od potencijalnog zlonamjernog softvera)
6. Pokrenite naredbu "Reset Waste Counters"
7. Odmah zaustavite Wireshark snimanje nakon što program kaže da isključite pisač

### Korak 2: Izvezite podatke
1. U Wiresharku, upišite ovaj točan filter u traku za prikaz filtera i pritisnite Enter:
   `usb.endpoint_address.direction == 0 && usb.transfer_type != 0x02`
   *(Ovo izolira `URB_BULK out` pakete poslane pisaču).*
2. Idite na **File** -> **Export Packet Dissections** -> **As C Arrays...**
3. Spremite datoteku s nazivom modela vašeg pisača (npr. `L3150.c`).

### Korak 3: Testirajte i otvorite Pull Request
1. Smjestite novu `L3150.c` datoteku u lokalni EWR `models/` direktorij.
2. Pokrenite EWR. Parser će automatski ukloniti Wireshark metapodatke i izvršiti podatke.
3. Ako se vaš waste brojač uspješno resetira, otvorite Pull Request i prenesite vašu `.c` datoteku u repozitorij kako bi je ostatak svijeta mogao koristiti!

Video vodič: https://youtu.be/PQzxifFqMsA

## Zahvale
Posebna zahvala projektu [reinkpy](https://codeberg.org/atufi/reinkpy) za njihovu fantastičnu bazu podataka. EWR koristi automatizirani GitHub Actions cjevovod za sinkronizaciju i pretvaranje njihove TOML baze podataka u naš C++ backend, spajajući njihovu ogromnu podršku za pisače s našim samostalnim C++ izvršnim okruženjem.

## ⚠️ Odricanje od odgovornosti
Manipuliranje hardverom putem sirovih USB paketa nosi inherentne rizike. EWR se pruža "kakav jest" bez ikakve garancije. Korištenjem ovog softvera prihvaćate punu odgovornost za vaš hardver.
