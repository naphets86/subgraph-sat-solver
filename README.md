# Subgraph-SAT-Solver

C++17-Implementierung einer Reduktionskette, die Instanzen des Erfüllbarkeitsproblems (SAT) auf das Subgraph-Problem abbildet und von einem externen Subgraph-Programm entscheiden lässt. Die theoretische Grundlage ist die wissenschaftliche Arbeit des Autors, siehe [Wissenschaftlicher Hintergrund](#wissenschaftlicher-hintergrund). Das zugehörige Subgraph-Programm ist ein eigenes Projekt: [subgraph](https://github.com/hjstephan86/subgraph).

## Inhaltsverzeichnis

- [Überblick](#überblick)
- [Reduktionskette](#reduktionskette)
- [Projektstruktur](#projektstruktur)
- [Voraussetzungen](#voraussetzungen)
- [Build](#build)
- [Tests](#tests)
- [Verwendung](#verwendung)
- [Schnittstelle zum Subgraph-Programm](#schnittstelle-zum-subgraph-programm)
- [Bekannte Einschränkungen](#bekannte-einschränkungen)
- [Wissenschaftlicher Hintergrund](#wissenschaftlicher-hintergrund)
- [Erwerb](#erwerb)

## Überblick

Der Solver liest eine Formel im DIMACS-CNF-Format ein, überführt sie über 3-SAT in ein Clique-Problem und formuliert dieses als Subgraph-Instanz. Die Entscheidung, ob der Zielgraph im Quellgraphen enthalten ist, trifft ein externes Programm. Das Ergebnis wird als erfüllbar oder unerfüllbar interpretiert.

Das Repository stellt bereit:

- ein Kommandozeilenprogramm (`SubgraphSATSolver`),
- eine statische Bibliothek (`satlib`) zur Einbindung in eigene Projekte,
- eine Testsuite (`sat-tests`).

Die Bewertung der Komplexität des Gesamtverfahrens ist Gegenstand der zugrunde liegenden Arbeit. Dieses Repository liefert die Implementierung der Reduktionskette und führt keinen eigenen Beweis.

## Reduktionskette

```
DIMACS-CNF  →  3-SAT  →  Clique  →  Subgraph-Instanz  →  externes Subgraph-Programm
```

| Stufe | Beschreibung | Komponente |
|-------|--------------|------------|
| CNF → 3-SAT | Klauseln mit mehr als drei Literalen werden mit Hilfsvariablen aufgespalten; Vereinfachung (Duplikate, Tautologien, Unit-Propagation) | `CNFFormula::to_3sat`, `simplify` |
| 3-SAT → Clique | Ein Knoten je Paar (Klausel, Literal). Kanten zwischen Knoten verschiedener Klauseln, sofern die Literale einander nicht widersprechen. Eine Clique der Größe m (Anzahl der Klauseln) existiert genau dann, wenn die Formel erfüllbar ist | `ThreeSATToCliqueReducer` |
| Clique → Subgraph | Der Clique-Graph ist der Quellgraph, der vollständige Graph K_k der Zielgraph | `CliqueToSubgraphReducer` |
| Orchestrierung | Führt alle Stufen aus und serialisiert beide Graphen als JSON | `SATToSubgraphReducer` |
| Subgraph-Aufruf | Startet das externe Programm, wertet dessen Ausgabe aus und validiert optional die Belegung | `SubgraphSATSolver` |

## Projektstruktur

| Datei | Inhalt |
|-------|--------|
| `Cnf.h`, `Cnf.cpp` | DIMACS-Parser, CNF-Datenstruktur, Statistiken, 3-SAT-Konvertierung, Vereinfachung, `SATAssignment`, `SATAnalyzer` |
| `Reduction.h`, `Reduction.cpp` | Graphklasse (Adjazenzmatrix, JSON-Export), die drei Reduktionsstufen, Extraktor für Belegungen |
| `SubgraphSATSolver.h`, `SubgraphSATSolver.cpp` | Solver-Klasse, Konfiguration, Ergebnistyp, Hilfsfunktionen (Dateiein- und -ausgabe, Validierung) |
| `main.cpp` | Kommandozeilenprogramm |
| `tests/TestCnf.cpp` | Testsuite (15 Tests) |
| `doc/report.md` | Coverage-Bericht |
| `doc/tests.txt` | Ausgabe eines Testlaufs |
| `CMakeLists.txt` | Build-Konfiguration |
| `build/` | Enthält die vorkompilierten Artefakte `SubgraphSATSolver.exe` und `libsatlib.a` (Windows, MinGW) |

## Voraussetzungen

- CMake ab Version 3.16
- Compiler mit C++17-Unterstützung (GCC, Clang, MinGW oder MSVC)
- [nlohmann/json](https://github.com/nlohmann/json) ab Version 3.2.0. Wird die Bibliothek nicht gefunden, lädt CMake Version 3.11.2 automatisch herunter (Internetzugang erforderlich).
- Ein ausführbares Subgraph-Programm, das die [beschriebene Schnittstelle](#schnittstelle-zum-subgraph-programm) erfüllt

Unter Windows können CMake und MinGW über folgende Quellen bezogen werden:

- CMake: https://cmake.org/download/
- MinGW-Installer: https://github.com/Vuniverse0/mingwInstaller/releases/download/1.2.1/mingwInstaller.exe

## Build

Alle Befehle werden im Verzeichnis `build` ausgeführt:

```
mkdir build
cd build
```

### Windows mit MinGW (PowerShell)

```powershell
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j 4
```

### Linux und macOS

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j 4
```

Erzeugte Ziele:

| Ziel | Typ |
|------|-----|
| `SubgraphSATSolver` | Kommandozeilenprogramm |
| `satlib` | statische Bibliothek |
| `sat-tests` | Testprogramm |

Hinweis: Für GCC, Clang und MinGW ist `-march=native` gesetzt. Die erzeugten Binärdateien sind daher auf die Architektur des Build-Rechners abgestimmt und nicht beliebig übertragbar.

Mit `cmake --install .` werden das Programm und die Bibliothek nach `bin` bzw. `lib` und die Header nach `include/sat` installiert.

## Tests

```powershell
.\sat-tests.exe
```

```bash
./sat-tests
```

Alternativ über CTest:

```
ctest --output-on-failure
```

Die Testsuite umfasst 15 Tests in sieben Kategorien: CNF-Parsing und -Manipulation, SAT-Belegungen, SAT-Analyse, Graphoperationen und JSON-Export, Reduktionskette, Solver-Hilfsfunktionen sowie JSON- und Kommandozeilen-Parsing.

Der Coverage-Bericht in `doc/report.md` weist für den Stand vom 9. Juli 2026 eine Zeilenabdeckung von 87,0 % aus. Die schwächsten Bereiche sind die Anbindung des externen Programms und Fehlerpfade. Ein Beispiel für die Ausgabe eines Testlaufs liegt in `doc/tests.txt`.

Test 13 prüft das Fehlerverhalten ohne verfügbares Subgraph-Programm und erwartet den Status `SAT_ERROR`. Die dabei ausgegebene Fehlermeldung des Betriebssystems gehört zum erwarteten Verlauf.

## Verwendung

### Kommandozeile

```
SubgraphSATSolver <input.cnf> [Optionen]
```

| Option | Beschreibung | Standardwert |
|--------|--------------|--------------|
| `-o`, `--output <Datei>` | Lösung in Datei schreiben | keine |
| `--subgraph-binary <Pfad>` | Pfad zum Subgraph-Programm | `./subgraph` |
| `--temp-dir <Verzeichnis>` | Verzeichnis für temporäre JSON-Dateien | `/tmp` |
| `--timeout <Sekunden>` | Zeitlimit für das Subgraph-Programm | `60` |
| `-v`, `--verbose` | ausführliche Ausgabe | aus |
| `--no-validate` | Validierung der Belegung überspringen | Validierung aktiv |
| `--stats` | Statistik der Eingabeformel ausgeben | aus |
| `-h`, `--help` | Hilfe anzeigen | |

Unter Windows existiert `/tmp` in der Regel nicht. Dort ist `--temp-dir` mit einem gültigen Verzeichnis anzugeben, beispielsweise `--temp-dir C:\Temp`.

Beispiel:

```powershell
.\SubgraphSATSolver.exe benchmark.cnf --subgraph-binary C:\tools\subgraph.exe --temp-dir C:\Temp -o solution.txt -v
```

### Rückgabecodes

| Code | Bedeutung |
|------|-----------|
| 10 | Formel erfüllbar |
| 20 | Formel nicht als erfüllbar festgestellt (unerfüllbar, unbekannt oder Solver-Fehler) |
| 1 | Fehler bei Argumenten oder Ausnahme beim Einlesen |

### Ausgabeformat

Mit `-o` wird eine Lösungsdatei im DIMACS-Stil geschrieben: eine Kommentarzeile, eine Statuszeile (`s SATISFIABLE`, `s UNSATISFIABLE` oder `s UNKNOWN`) und bei erfüllbaren Formeln eine mit `0` abgeschlossene Belegungszeile (`v ...`).

### Einbindung als Bibliothek

```cpp
#include "SubgraphSATSolver.h"

using namespace sat;

int main() {
    SATSolverConfig config;
    config.subgraph_binary_path = "./subgraph";
    config.temp_dir = "/tmp";
    config.subgraph_timeout_sec = 60;
    config.verbose = false;
    config.validate_result = true;

    SubgraphSATSolver solver(config);

    SATSolverResult result = solver.solve_dimacs_file("formula.cnf");
    // alternativ: solve_dimacs_string(...) oder solve_formula(...)

    if (result.is_satisfiable()) {
        // result.assignment enthält die Belegung
    } else if (result.is_unsatisfiable()) {
        // keine erfüllende Belegung gefunden
    } else if (result.is_error()) {
        // result.error_message enthält die Ursache
    }
    return 0;
}
```

`SATSolverResult` enthält neben dem Status die Anzahl der Variablen und Klauseln sowie die Laufzeiten der einzelnen Phasen (Reduktion, Subgraph-Aufruf, Extraktion, gesamt) in Millisekunden.

## Schnittstelle zum Subgraph-Programm

Der Solver ruft das Subgraph-Programm als eigenen Prozess auf:

```
<subgraph-binary> <input.json> <output.json>
```

Eingabe (`input.json`): ein JSON-Objekt mit zwei Graphen. `G` ist der Quellgraph (Clique-Graph), `H` der Zielgraph (vollständiger Graph K_k). Beide Graphen sind ungerichtet und haben folgende Form:

```json
{
  "G": { "nodes": ["v0", "v1", "v2"], "edges": [["v0", "v1"], ["v1", "v2"]] },
  "H": { "nodes": ["v0", "v1"],       "edges": [["v0", "v1"]] }
}
```

Ausgabe (`output.json`): Das Programm muss mit Exit-Code 0 enden und die Zeichenfolge `"is_subgraph": true` (genau diese Schreibweise inklusive Leerzeichen) in die Ausgabedatei schreiben, wenn `H` in `G` enthalten ist. Jede andere Ausgabe wird als "kein Teilgraph" und damit als unerfüllbar gewertet. Ein Exit-Code ungleich 0 führt zum Status `SAT_ERROR`.

Das Python-Projekt [subgraph](https://github.com/hjstephan86/subgraph) arbeitet auf Adjazenzmatrizen (`numpy`) und stellt keine Kommandozeilenschnittstelle mit diesem JSON-Format bereit. Für die Anbindung ist ein Adapter erforderlich, der das JSON einliest, die Graphen in Adjazenzmatrizen überführt, den Vergleich ausführt und das Ergebnis im oben beschriebenen Format schreibt.

## Bekannte Einschränkungen

- **Extraktion der Belegung:** `SubgraphToSATExtractor` wertet das Mapping der Clique-Knoten derzeit nicht aus. Der Solver setzt nach einem positiven Subgraph-Ergebnis alle Variablen auf wahr. Ist die Validierung aktiv (Standard), wird diese Belegung gegen die Formel geprüft; erfüllt sie die Formel nicht, endet der Lauf mit `SAT_ERROR` ("Assignment validation failed"). Eine aus dem Mapping abgeleitete Belegung ist noch umzusetzen.
- **Antwortformat des Subgraph-Programms:** Die Auswertung prüft ausschließlich auf die Zeichenfolge `"is_subgraph": true`. Das Mapping selbst wird nicht eingelesen.
- **Größe der Reduktion:** Der Clique-Graph besitzt einen Knoten je Literal-Vorkommen in der 3-SAT-Formel. Der Speicherbedarf der Adjazenzmatrix wächst quadratisch mit der Literalzahl.
- **Plattformen:** Die vorkompilierten Artefakte in `build/` stammen von Windows mit MinGW. Die Linux-Variante der Prozessanbindung (`fork`/`execl`) ist implementiert, wird von der Testsuite aber nicht gegen ein echtes Subgraph-Programm geprüft.
- **Zeitlimit:** Die Option `--timeout` wird an die Konfiguration übergeben. Eine Durchsetzung beim Prozessaufruf ist in `call_subgraph_binary` nicht implementiert.

## Wissenschaftlicher Hintergrund

Die Arbeit, auf der dieser Solver beruht, ist hier veröffentlicht: https://github.com/naphets86/odd/tree/main/science/lsat/science

Die Reduktion von 3-SAT auf das Clique-Problem folgt der klassischen Konstruktion der Komplexitätstheorie (Karp, Cook). Das Subgraph-Programm implementiert den Subgraph Algorithmus mit Spaltensignaturen und zyklischen Rotationen mit O(n³) Laufzeit. Die Aussagen zur Korrektheit und Laufzeit des Gesamtverfahrens sind in der Arbeit dargestellt und gelten nur im Rahmen dieser Darstellung.

## Erwerb

Der Preis für diese Software beträgt 3.145.000,00 EUR.

### Zahlungsinformationen

Name: Stephan Epp  
IBAN: DE24 5003 1900 0012 5603 20  
BIC: BBVADEFFXXX
