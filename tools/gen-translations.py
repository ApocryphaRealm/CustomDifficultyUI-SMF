# -*- coding: utf-8 -*-
"""gen-translations.py - builds the eleven CustomDifficultyUI_<language>.txt files.

The English key list is extracted from the PATCHED source/UI.cpp, so it can never drift from the
code. Two shapes are read:

  * strings::TR("KEY", "English text") - the ordinary call;
  * a parallel `kFooKeys[] / kFooLabels[]` array pair - the six difficulty names and the seven
    log levels, whose entries are looked up by index at draw time rather than by a literal TR call.

The other ten languages are this project's own translations of that list, held below as one dict
per key. Writes REPO/dist/Interface/Translations/CustomDifficultyUI_<language>.txt for english +
the owner's ten languages: UTF-16LE with a BOM, one "$key<TAB>text" record per line, a literal
"\\n" for an embedded line break, CRLF records - the SKSE/SkyUI shape the Apocrypha Menu
Framework's Strings.cpp reads.

Untranslated on purpose, in every language: product and mod names (Skyrim, Custom Difficulty UI,
Blade and Blunt, Requiem, Apocrypha Menu Framework), file names (BladeAndBlunt.ini, CLAUDE.md),
INI keys and GameSetting names (bLevelBasedDifficulty, fCombatHealthRegenRateMult and the rest -
those never reach a translation file at all, they are drawn from untranslated literals), and the
ImGui id suffixes. The six difficulty names and Health/Magicka/Stamina use Skyrim's own localised
vocabulary.

Run: `python tools/gen-translations.py`.
"""
import io
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STEM = "CustomDifficultyUI"
LANGS = ["english", "japanese", "korean", "chinese", "russian",
         "german", "french", "spanish", "italian", "polish", "czech"]

TR_RE = re.compile(r'strings::TR\(\s*"((?:[^"\\]|\\.)+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')
KEYS_RE = re.compile(r'constexpr const char\* const k(\w+)Keys\[\]\s*=\s*\{([^}]*)\};')
LABELS_RE = re.compile(r'constexpr const char\* const k(\w+)Labels\[\]\s*=\s*\{([^}]*)\};')
LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s):
    return s.encode("latin-1", "backslashreplace").decode("unicode_escape") if "\\" in s else s


def read_keys():
    src = io.open(os.path.join(REPO, "source", "UI.cpp"), encoding="utf-8", newline="").read()
    order, texts = [], {}

    def add(key, text):
        if key in texts:
            if texts[key] != text:
                raise RuntimeError("key {!r} has two English texts: {!r} vs {!r}".format(key, texts[key], text))
            return
        order.append(key)
        texts[key] = text

    for m in TR_RE.finditer(src):
        add(unescape(m.group(1)), unescape(m.group(2)))

    labels = {m.group(1): m.group(2) for m in LABELS_RE.finditer(src)}
    for m in KEYS_RE.finditer(src):
        name = m.group(1)
        if name not in labels:
            raise RuntimeError("k{}Keys has no matching k{}Labels array".format(name, name))
        ks = [unescape(x) for x in LIT_RE.findall(m.group(2))]
        vs = [unescape(x) for x in LIT_RE.findall(labels[name])]
        if len(ks) != len(vs):
            raise RuntimeError("k{}Keys ({}) and k{}Labels ({}) length mismatch".format(name, len(ks), name, len(vs)))
        for k, v in zip(ks, vs):
            add(k, v)

    return order, texts


# ------------------------------------------------------------------------------------------------
# The project's own translations, one dict per key, in the order the page draws them.
# ------------------------------------------------------------------------------------------------
def T(ja, ko, zh, ru, de, fr, es, it, pl, cs):
    return {"japanese": ja, "korean": ko, "chinese": zh, "russian": ru, "german": de,
            "french": fr, "spanish": es, "italian": it, "polish": pl, "czech": cs}


TRANSLATIONS = {}

# --- marks and the shared "not available" row --------------------------------------------------
TRANSLATIONS["CDUI_NudgeMark"] = T("<-->", "<-->", "<-->", "<-->", "<-->", "<-->", "<-->", "<-->", "<-->", "<-->")
TRANSLATIONS["CDUI_HelpMark"] = T("(?)", "(?)", "(?)", "(?)", "(?)", "(?)", "(?)", "(?)", "(?)", "(?)")
TRANSLATIONS["CDUI_NotAvailable"] = T(
    "%s (%s) ― このビルドでは利用できません。GameSetting が見つかりませんでした",
    "%s (%s) ― 이 빌드에서는 사용할 수 없습니다. GameSetting을 찾지 못했습니다",
    "%s（%s）― 此版本不可用；未找到该 GameSetting",
    "%s (%s) — недоступно в этой сборке; GameSetting не найден",
    "%s (%s) - in diesem Build nicht verfügbar; das GameSetting wurde nicht gefunden",
    "%s (%s) - non disponible dans cette version ; le GameSetting est introuvable",
    "%s (%s): no disponible en esta versión; no se encontró el GameSetting",
    "%s (%s) - non disponibile in questa build; il GameSetting non è stato trovato",
    "%s (%s) - niedostępne w tej wersji; nie znaleziono GameSetting",
    "%s (%s) - v tomto sestavení není k dispozici; GameSetting nebyl nalezen")

# --- one difficulty's pair ---------------------------------------------------------------------
TRANSLATIONS["CDUI_HelpToYou"] = T(
    "この難易度で敵から受けるダメージに掛かる倍率です。Ctrl+クリックで数値を直接入力できます。",
    "이 난이도에서 적에게 받는 피해에 적용되는 배율입니다. Ctrl+클릭으로 값을 직접 입력할 수 있습니다.",
    "该难度下敌人对你造成伤害的倍率。Ctrl+点击可直接输入数值。",
    "Множитель урона, который враги наносят вам на этой сложности. Ctrl+щелчок — ввести значение вручную.",
    "Schadensmultiplikator für Treffer, die Gegner bei dir landen, auf dieser Schwierigkeit. Strg+Klick, um einen Wert einzugeben.",
    "Multiplicateur des dégâts que les ennemis vous infligent à cette difficulté. Ctrl+clic pour saisir une valeur.",
    "Multiplicador del daño que los enemigos te infligen en esta dificultad. Ctrl+clic para escribir un valor.",
    "Moltiplicatore dei danni che i nemici ti infliggono a questa difficoltà. Ctrl+clic per digitare un valore.",
    "Mnożnik obrażeń zadawanych tobie przez wrogów na tym poziomie trudności. Ctrl+kliknięcie, aby wpisać wartość.",
    "Násobitel poškození, které ti nepřátelé způsobí na této obtížnosti. Ctrl+klik pro zadání hodnoty.")
TRANSLATIONS["CDUI_HelpByYou"] = T(
    "この難易度で敵に与えるダメージに掛かる倍率です。Ctrl+クリックで数値を直接入力できます。",
    "이 난이도에서 적에게 주는 피해에 적용되는 배율입니다. Ctrl+클릭으로 값을 직접 입력할 수 있습니다.",
    "该难度下你对敌人造成伤害的倍率。Ctrl+点击可直接输入数值。",
    "Множитель урона, который вы наносите врагам на этой сложности. Ctrl+щелчок — ввести значение вручную.",
    "Schadensmultiplikator für Treffer, die du bei Gegnern landest, auf dieser Schwierigkeit. Strg+Klick, um einen Wert einzugeben.",
    "Multiplicateur des dégâts que vous infligez aux ennemis à cette difficulté. Ctrl+clic pour saisir une valeur.",
    "Multiplicador del daño que infliges a los enemigos en esta dificultad. Ctrl+clic para escribir un valor.",
    "Moltiplicatore dei danni che infliggi ai nemici a questa difficoltà. Ctrl+clic per digitare un valore.",
    "Mnożnik obrażeń zadawanych wrogom na tym poziomie trudności. Ctrl+kliknięcie, aby wpisać wartość.",
    "Násobitel poškození, které způsobíš nepřátelům na této obtížnosti. Ctrl+klik pro zadání hodnoty.")
TRANSLATIONS["CDUI_LoadedWith"] = T(
    "    読み込み時: 被ダメージ x%.2f、与ダメージ x%.2f",
    "    불러온 값: 받는 피해 x%.2f, 주는 피해 x%.2f",
    "    载入时为：受到伤害 x%.2f，造成伤害 x%.2f",
    "    при загрузке: x%.2f по вам, x%.2f от вас",
    "    geladen mit: x%.2f auf dich, x%.2f von dir",
    "    chargé avec : x%.2f sur vous, x%.2f par vous",
    "    cargado con: x%.2f hacia ti, x%.2f por ti",
    "    caricato con: x%.2f su di te, x%.2f da te",
    "    wczytano z: x%.2f na ciebie, x%.2f od ciebie",
    "    načteno s: x%.2f na tebe, x%.2f od tebe")

# --- the difficulty section --------------------------------------------------------------------
TRANSLATIONS["CDUI_OverhaulLoaded"] = T(
    "%s が読み込まれています。その被ダメージ倍率が各ペアの下に表示される読み込み値です。「有効」をオンにするまでは何も変更されません。オンにすると、この MOD が最後に書き込み、そちらを上書きします。",
    "%s이(가) 로드되어 있습니다. 그 피해 배율이 각 쌍 아래에 표시된 불러온 값입니다. '사용'을 켜기 전까지는 아무것도 건드리지 않으며, 켜면 이 모드가 마지막에 기록하여 덮어씁니다.",
    "已加载 %s。其伤害倍率就是每组下方显示的载入值；在启用之前这里不会改动它们，启用后本模组会最后写入并覆盖它们。",
    "Загружен %s. Его множители урона — это значения загрузки, показанные под каждой парой; пока «Включено» выключено, здесь ничего не меняется, а при включении этот мод пишет последним и заменяет их.",
    "%s ist geladen. Seine Schadensmultiplikatoren sind die geladenen Werte unter jedem Paar; hier wird nichts angerührt, bis Aktiviert an ist - dann schreibt diese Mod zuletzt und überschreibt sie.",
    "%s est chargé. Ses multiplicateurs de dégâts sont les valeurs chargées affichées sous chaque paire ; rien n'y touche tant qu'Activé est désactivé - une fois activé, ce mod écrit en dernier et les remplace.",
    "%s está cargado. Sus multiplicadores de daño son los valores cargados que se muestran bajo cada par; nada los toca hasta que Activado esté encendido: entonces este mod escribe el último y los sustituye.",
    "%s è caricata. I suoi moltiplicatori di danno sono i valori caricati mostrati sotto ogni coppia; nulla li tocca finché Attivo non è acceso - allora questa mod scrive per ultima e li sostituisce.",
    "Wczytano %s. Jego mnożniki obrażeń to wartości wczytane pokazane pod każdą parą; nic ich nie zmienia, dopóki Włączone jest wyłączone - po włączeniu ta modyfikacja zapisuje jako ostatnia i je zastępuje.",
    "%s je načten. Jeho násobitele poškození jsou načtené hodnoty zobrazené pod každou dvojicí; dokud není Zapnuto, nic se jich nedotkne - po zapnutí tento mod zapisuje jako poslední a nahradí je.")
TRANSLATIONS["CDUI_BBScaling"] = T(
    "BladeAndBlunt.ini の bLevelBasedDifficulty が true です。その DLL もレベル 10～50 で倍率を段階的に変更します。この MOD を有効にしている間は false にしてください ― 同じ値を二つが書き込む状態は安定しません。下の「レベルによる難易度」が同じ役割を果たします。",
    "BladeAndBlunt.ini의 bLevelBasedDifficulty가 true입니다. 해당 DLL도 레벨 10~50에서 배율을 단계적으로 바꿉니다. 이 모드를 사용하는 동안에는 false로 두세요 ― 한 값을 둘이 쓰면 결코 안정적이지 않습니다. 아래의 '레벨에 따른 난이도'가 같은 일을 합니다.",
    "BladeAndBlunt.ini 中 bLevelBasedDifficulty 为 true：它的 DLL 也会在 10 到 50 级之间调整倍率。启用本模组时请将其设为 false ― 两个写入者共用一个数值永远不稳定。下方的“按等级设定难度”做的是同一件事。",
    "В BladeAndBlunt.ini параметр bLevelBasedDifficulty равен true: его DLL тоже меняет множители на уровнях с 10 по 50. Пока этот мод включён, поставьте false — два писателя на одно значение никогда не бывают стабильны. «Сложность по уровню» ниже делает то же самое.",
    "In BladeAndBlunt.ini steht bLevelBasedDifficulty = true: dessen DLL stuft die Multiplikatoren auf den Stufen 10 bis 50 ebenfalls ab. Setze es auf false, solange diese Mod aktiv ist - zwei Schreiber auf einem Wert sind nie stabil. Schwierigkeit nach Stufe unten leistet dasselbe.",
    "BladeAndBlunt.ini a bLevelBasedDifficulty = true : sa DLL modifie aussi les multiplicateurs aux niveaux 10 à 50. Mettez-le à false tant que ce mod est activé - deux écrivains sur une même valeur ne sont jamais stables. Difficulté selon le niveau, ci-dessous, fait la même chose.",
    "BladeAndBlunt.ini tiene bLevelBasedDifficulty = true: su DLL también escalona los multiplicadores entre los niveles 10 y 50. Ponlo en false mientras este mod esté activado: dos escritores sobre un mismo valor nunca son estables. Dificultad por nivel, más abajo, hace lo mismo.",
    "BladeAndBlunt.ini ha bLevelBasedDifficulty = true: la sua DLL modifica i moltiplicatori anche ai livelli da 10 a 50. Impostalo su false mentre questa mod è attiva - due scrittori su un solo valore non sono mai stabili. Difficoltà per livello, qui sotto, fa lo stesso lavoro.",
    "W BladeAndBlunt.ini bLevelBasedDifficulty = true: jego DLL również zmienia mnożniki na poziomach od 10 do 50. Ustaw go na false, gdy ta modyfikacja jest włączona - dwóch zapisujących jedną wartość nigdy nie jest stabilne. Trudność według poziomu poniżej robi to samo.",
    "BladeAndBlunt.ini má bLevelBasedDifficulty = true: jeho DLL také mění násobitele na úrovních 10 až 50. Nastav ho na false, dokud je tento mod zapnutý - dva zapisovatelé jedné hodnoty nikdy nejsou stabilní. Obtížnost podle úrovně níže dělá totéž.")
TRANSLATIONS["CDUI_BBNotFound"] = T(
    "BladeAndBlunt.ini が見つからなかったため、bLevelBasedDifficulty を読み取れませんでした。true になっている場合は、この MOD を有効にしている間 false にしてください。",
    "BladeAndBlunt.ini을 찾지 못해 bLevelBasedDifficulty를 읽을 수 없었습니다. true라면 이 모드를 사용하는 동안 false로 두세요.",
    "未找到 BladeAndBlunt.ini，因此无法读取其 bLevelBasedDifficulty。如果它为 true，请在启用本模组时将其设为 false。",
    "Файл BladeAndBlunt.ini не найден, поэтому его bLevelBasedDifficulty прочитать не удалось. Если он равен true, поставьте false, пока этот мод включён.",
    "BladeAndBlunt.ini wurde nicht gefunden, daher konnte bLevelBasedDifficulty nicht gelesen werden. Falls es true ist, setze es auf false, solange diese Mod aktiv ist.",
    "BladeAndBlunt.ini est introuvable, son bLevelBasedDifficulty n'a donc pas pu être lu. S'il vaut true, mettez-le à false tant que ce mod est activé.",
    "No se encontró BladeAndBlunt.ini, así que no se pudo leer su bLevelBasedDifficulty. Si está en true, ponlo en false mientras este mod esté activado.",
    "BladeAndBlunt.ini non è stato trovato, quindi il suo bLevelBasedDifficulty non ha potuto essere letto. Se è true, impostalo su false mentre questa mod è attiva.",
    "Nie znaleziono BladeAndBlunt.ini, więc nie udało się odczytać bLevelBasedDifficulty. Jeśli jest ustawione na true, zmień je na false, gdy ta modyfikacja jest włączona.",
    "BladeAndBlunt.ini nebyl nalezen, takže jeho bLevelBasedDifficulty nešlo přečíst. Pokud je true, nastav ho na false, dokud je tento mod zapnutý.")
TRANSLATIONS["CDUI_Enabled"] = T(
    "有効", "사용", "启用", "Включено", "Aktiviert",
    "Activé", "Activado", "Attivo", "Włączone", "Zapnuto")
TRANSLATIONS["CDUI_HelpEnabled"] = T(
    "オフでは何も書き込みません。ゲームが読み込んだ倍率（バニラ、またはオーバーホールのもの）はそのまま残り、オフに戻せばそれらが戻ります。オンにすると下のペアが書き込まれ、ゲームはプレイ中の難易度のペアを読み取ります。",
    "끄면 아무것도 기록하지 않습니다. 게임이 불러온 배율(바닐라 또는 오버홀의 값)이 그대로 유지되며, 끄면 그 값이 되돌아옵니다. 켜면 아래의 쌍이 기록되고, 게임은 플레이 중인 난이도의 쌍을 읽습니다.",
    "关闭时不写入任何内容：游戏载入的倍率（原版或某个大修的）保持原样，关闭也会把它们交还。开启时会写入下面的数值对，游戏读取你所玩难度对应的那一对。",
    "Выключено — ничего не записывается: множители, с которыми загрузилась игра (ванильные или от оверхола), остаются как есть, а выключение возвращает их. Включено — записываются пары ниже, и игра читает пару для той сложности, на которой вы играете.",
    "Aus schreibt nichts: Die Multiplikatoren, mit denen dein Spiel geladen hat (Vanilla oder die einer Überarbeitung), bleiben genau so, und Ausschalten gibt sie zurück. An: Die Paare unten werden geschrieben, und das Spiel liest das Paar für die Schwierigkeit, die du spielst.",
    "Désactivé n'écrit rien : les multiplicateurs avec lesquels votre jeu s'est chargé (vanilla, ou ceux d'une refonte) restent tels quels, et la désactivation les rend. Activé : les paires ci-dessous sont écrites, et le jeu lit la paire de la difficulté à laquelle vous jouez.",
    "Desactivado no escribe nada: los multiplicadores con los que cargó tu juego (vanilla, o los de una revisión) se quedan tal cual, y al desactivar se devuelven. Activado: se escriben los pares de abajo y el juego lee el par de la dificultad en la que juegas.",
    "Disattivato non scrive nulla: i moltiplicatori con cui il gioco si è caricato (vanilla, o quelli di una revisione) restano come sono, e disattivando vengono restituiti. Attivo: le coppie qui sotto vengono scritte e il gioco legge la coppia della difficoltà a cui giochi.",
    "Wyłączone nic nie zapisuje: mnożniki, z którymi gra się wczytała (podstawowe lub z jakiejś przebudowy), pozostają bez zmian, a wyłączenie je oddaje. Włączone: zapisywane są pary poniżej, a gra czyta parę dla poziomu trudności, na którym grasz.",
    "Vypnuto nic nezapisuje: násobitele, se kterými se hra načetla (základní nebo z nějakého předělání), zůstanou tak, jak jsou, a vypnutí je vrátí. Zapnuto: zapisují se dvojice níže a hra čte dvojici pro obtížnost, na které hraješ.")
TRANSLATIONS["CDUI_SharedPair"] = T(
    "すべての難易度で同じペアを使う",
    "모든 난이도에 하나의 쌍 사용",
    "所有难度共用一组数值",
    "Одна пара для всех сложностей",
    "Ein Paar für jede Schwierigkeit",
    "Une seule paire pour toutes les difficultés",
    "Un solo par para todas las dificultades",
    "Una sola coppia per ogni difficoltà",
    "Jedna para dla każdego poziomu trudności",
    "Jedna dvojice pro všechny obtížnosti")
TRANSLATIONS["CDUI_HelpSharedPair"] = T(
    "オン: 下の一組が六つすべての難易度に書き込まれるため、ゲームの難易度設定はダメージに影響しなくなります。オフ: 難易度ごとに独自のペアを持ちます。",
    "켬: 아래의 한 쌍이 여섯 난이도 모두에 기록되어 게임의 난이도 설정이 피해에 영향을 주지 않습니다. 끔: 난이도마다 고유한 쌍을 가집니다.",
    "开启：下面这一组会写入全部六个难度，因此游戏的难度设定不再影响伤害。关闭：每个难度各有自己的一组数值。",
    "Вкл.: пара ниже записывается для всех шести сложностей, поэтому настройка сложности не влияет на урон. Выкл.: у каждой сложности своя пара.",
    "An: Das eine Paar unten wird für alle sechs Schwierigkeiten geschrieben, sodass die Schwierigkeitseinstellung des Spiels keinen Unterschied für den Schaden macht. Aus: Jede Schwierigkeit hat ihr eigenes Paar.",
    "Activé : la paire unique ci-dessous est écrite pour les six difficultés, la difficulté du jeu ne change donc rien aux dégâts. Désactivé : chaque difficulté a sa propre paire.",
    "Activado: el único par de abajo se escribe para las seis dificultades, así que el ajuste de dificultad no cambia el daño. Desactivado: cada dificultad tiene su propio par.",
    "Attivo: l'unica coppia qui sotto viene scritta per tutte e sei le difficoltà, quindi l'impostazione di difficoltà non cambia i danni. Disattivo: ogni difficoltà ha la sua coppia.",
    "Włączone: jedna para poniżej jest zapisywana dla wszystkich sześciu poziomów trudności, więc ustawienie trudności nie zmienia obrażeń. Wyłączone: każdy poziom ma własną parę.",
    "Zapnuto: jediná dvojice níže se zapíše pro všech šest obtížností, takže nastavení obtížnosti nemá na poškození vliv. Vypnuto: každá obtížnost má vlastní dvojici.")
TRANSLATIONS["CDUI_DamageToYou"] = T(
    "被ダメージ", "받는 피해", "受到的伤害", "Урон по вам", "Schaden auf dich",
    "Dégâts sur vous", "Daño hacia ti", "Danni su di te", "Obrażenia na ciebie", "Poškození na tebe")
TRANSLATIONS["CDUI_HelpSharedToYou"] = T(
    "すべての難易度で、敵から受けるダメージに掛かる倍率です。Ctrl+クリックで数値を直接入力できます。",
    "모든 난이도에서 적에게 받는 피해에 적용되는 배율입니다. Ctrl+클릭으로 값을 직접 입력할 수 있습니다.",
    "在所有难度下，敌人对你造成伤害的倍率。Ctrl+点击可直接输入数值。",
    "Множитель урона, который враги наносят вам, на всех сложностях. Ctrl+щелчок — ввести значение вручную.",
    "Schadensmultiplikator für Treffer, die Gegner bei dir landen, auf jeder Schwierigkeit. Strg+Klick, um einen Wert einzugeben.",
    "Multiplicateur des dégâts que les ennemis vous infligent, à toutes les difficultés. Ctrl+clic pour saisir une valeur.",
    "Multiplicador del daño que los enemigos te infligen, en todas las dificultades. Ctrl+clic para escribir un valor.",
    "Moltiplicatore dei danni che i nemici ti infliggono, a ogni difficoltà. Ctrl+clic per digitare un valore.",
    "Mnożnik obrażeń zadawanych tobie przez wrogów na każdym poziomie trudności. Ctrl+kliknięcie, aby wpisać wartość.",
    "Násobitel poškození, které ti nepřátelé způsobí, na každé obtížnosti. Ctrl+klik pro zadání hodnoty.")
TRANSLATIONS["CDUI_DamageByYou"] = T(
    "与ダメージ", "주는 피해", "造成的伤害", "Урон от вас", "Schaden von dir",
    "Dégâts par vous", "Daño por ti", "Danni da te", "Obrażenia od ciebie", "Poškození od tebe")
TRANSLATIONS["CDUI_HelpSharedByYou"] = T(
    "すべての難易度で、敵に与えるダメージに掛かる倍率です。Ctrl+クリックで数値を直接入力できます。",
    "모든 난이도에서 적에게 주는 피해에 적용되는 배율입니다. Ctrl+클릭으로 값을 직접 입력할 수 있습니다.",
    "在所有难度下，你对敌人造成伤害的倍率。Ctrl+点击可直接输入数值。",
    "Множитель урона, который вы наносите врагам, на всех сложностях. Ctrl+щелчок — ввести значение вручную.",
    "Schadensmultiplikator für Treffer, die du bei Gegnern landest, auf jeder Schwierigkeit. Strg+Klick, um einen Wert einzugeben.",
    "Multiplicateur des dégâts que vous infligez aux ennemis, à toutes les difficultés. Ctrl+clic pour saisir une valeur.",
    "Multiplicador del daño que infliges a los enemigos, en todas las dificultades. Ctrl+clic para escribir un valor.",
    "Moltiplicatore dei danni che infliggi ai nemici, a ogni difficoltà. Ctrl+clic per digitare un valore.",
    "Mnożnik obrażeń zadawanych wrogom na każdym poziomie trudności. Ctrl+kliknięcie, aby wpisać wartość.",
    "Násobitel poškození, které způsobíš nepřátelům, na každé obtížnosti. Ctrl+klik pro zadání hodnoty.")
TRANSLATIONS["CDUI_FillFrom"] = T(
    "表を埋める:", "표 채우기:", "填充表格来源：", "Заполнить таблицу из:", "Tabelle füllen aus:",
    "Remplir le tableau depuis :", "Rellenar la tabla desde:", "Riempi la tabella da:",
    "Wypełnij tabelę z:", "Naplnit tabulku z:")
TRANSLATIONS["CDUI_BtnLoaded"] = T(
    "読み込み値", "불러온 값", "载入值", "Значения загрузки", "Geladene Werte",
    "Valeurs chargées", "Valores cargados", "Valori caricati", "Wartości wczytane", "Načtené hodnoty")
TRANSLATIONS["CDUI_StatusLoaded"] = T(
    "このゲームが読み込んだ値を表に入れました。保存するには「保存」を押してください。",
    "이 게임이 불러온 값을 표에 넣었습니다. 유지하려면 저장을 누르세요.",
    "表格已填入本次游戏载入的数值。按“保存”以保留它们。",
    "В таблице значения, с которыми загрузилась игра. Нажмите «Сохранить», чтобы оставить их.",
    "Die Tabelle enthält die Werte, mit denen dieses Spiel geladen hat. Drücke Speichern, um sie zu behalten.",
    "Le tableau contient les valeurs avec lesquelles ce jeu s'est chargé. Appuyez sur Enregistrer pour les conserver.",
    "La tabla contiene los valores con los que cargó este juego. Pulsa Guardar para conservarlos.",
    "La tabella contiene i valori con cui questo gioco si è caricato. Premi Salva per mantenerli.",
    "Tabela zawiera wartości, z którymi wczytała się ta gra. Naciśnij Zapisz, aby je zachować.",
    "Tabulka obsahuje hodnoty, se kterými se tato hra načetla. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CDUI_HelpLoaded"] = T(
    "読み込み時にゲームが保持していた値 ― バニラ、または導入しているオーバーホールのものです。オーバーホールの数値を失わずに調整を始める出発点になります。",
    "게임이 불러올 때 가지고 있던 값 ― 바닐라이거나 사용 중인 오버홀의 값입니다. 오버홀의 수치를 잃지 않고 조정을 시작하는 출발점입니다.",
    "游戏载入时所持有的数值 ― 原版，或者你所使用的大修的数值。这是在不丢失大修数值的前提下开始调整的起点。",
    "То, что игра держит при загрузке — ванильные значения или значения оверхола, который вы используете. Отправная точка, чтобы настраивать оверхол, не теряя его чисел.",
    "Was dein Spiel beim Laden hält - Vanilla oder die Überarbeitung, die du spielst. Der Ausgangspunkt, um eine Überarbeitung zu justieren, ohne ihre Zahlen zu verlieren.",
    "Ce que votre jeu contient au chargement - vanilla, ou la refonte que vous utilisez. Le point de départ pour ajuster une refonte sans perdre ses chiffres.",
    "Lo que tu juego tiene al cargar: vanilla, o la revisión que uses. El punto de partida para ajustar una revisión sin perder sus números.",
    "Ciò che il tuo gioco contiene al caricamento - vanilla, o la revisione che usi. Il punto di partenza per regolare una revisione senza perderne i numeri.",
    "To, co gra ma przy wczytaniu - podstawowe wartości lub te z używanej przebudowy. Punkt wyjścia do strojenia przebudowy bez utraty jej liczb.",
    "To, co má hra při načtení - základní hodnoty nebo hodnoty předělání, které hraješ. Výchozí bod pro ladění předělání bez ztráty jeho čísel.")
TRANSLATIONS["CDUI_BtnVanilla"] = T(
    "バニラ", "바닐라", "原版", "Ванильные", "Vanilla",
    "Vanilla", "Vanilla", "Vanilla", "Podstawowe", "Základní")
TRANSLATIONS["CDUI_StatusVanilla"] = T(
    "Skyrim のバニラ値を表に入れました。保存するには「保存」を押してください。",
    "Skyrim의 바닐라 값을 표에 넣었습니다. 유지하려면 저장을 누르세요.",
    "表格已填入 Skyrim 的原版数值。按“保存”以保留它们。",
    "В таблице ванильные значения Skyrim. Нажмите «Сохранить», чтобы оставить их.",
    "Die Tabelle enthält Skyrims Vanilla-Werte. Drücke Speichern, um sie zu behalten.",
    "Le tableau contient les valeurs vanilla de Skyrim. Appuyez sur Enregistrer pour les conserver.",
    "La tabla contiene los valores vanilla de Skyrim. Pulsa Guardar para conservarlos.",
    "La tabella contiene i valori vanilla di Skyrim. Premi Salva per mantenerli.",
    "Tabela zawiera podstawowe wartości Skyrima. Naciśnij Zapisz, aby je zachować.",
    "Tabulka obsahuje základní hodnoty Skyrimu. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CDUI_StatusBB"] = T(
    "Blade and Blunt の値を表に入れました。保存するには「保存」を押してください。",
    "Blade and Blunt의 값을 표에 넣었습니다. 유지하려면 저장을 누르세요.",
    "表格已填入 Blade and Blunt 的数值。按“保存”以保留它们。",
    "В таблице значения Blade and Blunt. Нажмите «Сохранить», чтобы оставить их.",
    "Die Tabelle enthält die Werte von Blade and Blunt. Drücke Speichern, um sie zu behalten.",
    "Le tableau contient les valeurs de Blade and Blunt. Appuyez sur Enregistrer pour les conserver.",
    "La tabla contiene los valores de Blade and Blunt. Pulsa Guardar para conservarlos.",
    "La tabella contiene i valori di Blade and Blunt. Premi Salva per mantenerli.",
    "Tabela zawiera wartości Blade and Blunt. Naciśnij Zapisz, aby je zachować.",
    "Tabulka obsahuje hodnoty Blade and Blunt. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CDUI_HelpBB"] = T(
    "公開されているペア: 被ダメージはバニラと同じ、与ダメージは 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5。",
    "공개된 쌍: 받는 피해는 바닐라와 같고, 주는 피해는 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "其公布的数值对：受到伤害与原版相同，造成伤害为 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5。",
    "Его опубликованные пары: по вам — как в ванилле, от вас — 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Seine veröffentlichten Paare: auf dich wie Vanilla, von dir 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Ses paires publiées : sur vous comme en vanilla, par vous 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Sus pares publicados: hacia ti como en vanilla, por ti 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Le sue coppie pubblicate: su di te come in vanilla, da te 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Jego opublikowane pary: na ciebie jak w podstawce, od ciebie 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Jeho zveřejněné dvojice: na tebe jako základní hra, od tebe 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.")
TRANSLATIONS["CDUI_StatusRequiem"] = T(
    "Requiem の値を表に入れました。保存するには「保存」を押してください。",
    "Requiem의 값을 표에 넣었습니다. 유지하려면 저장을 누르세요.",
    "表格已填入 Requiem 的数值。按“保存”以保留它们。",
    "В таблице значения Requiem. Нажмите «Сохранить», чтобы оставить их.",
    "Die Tabelle enthält die Werte von Requiem. Drücke Speichern, um sie zu behalten.",
    "Le tableau contient les valeurs de Requiem. Appuyez sur Enregistrer pour les conserver.",
    "La tabla contiene los valores de Requiem. Pulsa Guardar para conservarlos.",
    "La tabella contiene i valori di Requiem. Premi Salva per mantenerli.",
    "Tabela zawiera wartości Requiem. Naciśnij Zapisz, aby je zachować.",
    "Tabulka obsahuje hodnoty Requiem. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CDUI_HelpRequiem"] = T(
    "すべての倍率が 1.0 ― Requiem では設計上、難易度設定でダメージが増減しません。",
    "모든 배율이 1.0 ― Requiem에서는 설계상 난이도 설정이 피해를 조정하지 않습니다.",
    "所有倍率均为 1.0 ― 在 Requiem 中，难度设定按设计不会缩放伤害。",
    "Все множители равны 1.0 — в Requiem настройка сложности по замыслу не масштабирует урон.",
    "Jeder Multiplikator 1.0 - in Requiem skaliert die Schwierigkeitseinstellung den Schaden bewusst nicht.",
    "Tous les multiplicateurs à 1.0 - dans Requiem, la difficulté ne modifie volontairement pas les dégâts.",
    "Todos los multiplicadores a 1.0: en Requiem el ajuste de dificultad no escala el daño por diseño.",
    "Tutti i moltiplicatori a 1.0 - in Requiem l'impostazione di difficoltà non scala i danni per scelta.",
    "Wszystkie mnożniki 1.0 - w Requiem ustawienie trudności z założenia nie skaluje obrażeń.",
    "Všechny násobitele 1.0 - v Requiem nastavení obtížnosti záměrně neškáluje poškození.")
TRANSLATIONS["CDUI_SectionsNote1"] = T(
    "以下の各セクションは Skyrim 本来の難易度のひとつです。",
    "아래의 각 구획은 Skyrim 자체의 난이도 중 하나입니다.",
    "下面每一节都对应 Skyrim 自身的一个难度。",
    "Каждый раздел ниже — одна из собственных сложностей Skyrim.",
    "Jeder Abschnitt unten ist eine von Skyrims eigenen Schwierigkeitsstufen.",
    "Chaque section ci-dessous correspond à l'une des difficultés propres à Skyrim.",
    "Cada sección de abajo es una de las dificultades propias de Skyrim.",
    "Ogni sezione qui sotto è una delle difficoltà proprie di Skyrim.",
    "Każda sekcja poniżej to jeden z własnych poziomów trudności Skyrima.",
    "Každá sekce níže je jedna z vlastních obtížností Skyrimu.")
TRANSLATIONS["CDUI_SectionsNote2"] = T(
    "ゲーム内で選んだ難易度のセクションのスライダーが使われます。",
    "게임에서 선택한 난이도의 구획에 있는 슬라이더가 사용됩니다.",
    "你在游戏中选择哪个难度，就使用该节的滑块。",
    "Какую сложность вы выберете в игре, слайдеры того раздела и будут применяться.",
    "Welche Schwierigkeit du im Spiel wählst, deren Abschnitt-Regler werden verwendet.",
    "La difficulté que vous choisissez en jeu utilise les curseurs de sa section.",
    "La dificultad que elijas en el juego usa los deslizadores de esa sección.",
    "La difficoltà che selezioni in gioco usa i cursori di quella sezione.",
    "Poziom trudności wybrany w grze korzysta z suwaków tej sekcji.",
    "Obtížnost, kterou zvolíš ve hře, používá posuvníky své sekce.")
TRANSLATIONS["CDUI_ByLevelHeader"] = T(
    "レベルによる難易度", "레벨에 따른 난이도", "按等级设定难度", "Сложность по уровню",
    "Schwierigkeit nach Stufe", "Difficulté selon le niveau", "Dificultad por nivel",
    "Difficoltà per livello", "Trudność według poziomu", "Obtížnost podle úrovně")
TRANSLATIONS["CDUI_ByLevel"] = T(
    "自分のレベルからゲームの難易度を決める",
    "내 레벨에 따라 게임 난이도를 설정",
    "根据你的等级设定游戏难度",
    "Задавать сложность игры по вашему уровню",
    "Die Spielschwierigkeit von deiner Stufe bestimmen lassen",
    "Définir la difficulté du jeu d'après votre niveau",
    "Fijar la dificultad del juego según tu nivel",
    "Imposta la difficoltà del gioco dal tuo livello",
    "Ustaw trudność gry na podstawie twojego poziomu",
    "Nastavovat obtížnost hry podle tvé úrovně")
TRANSLATIONS["CDUI_HelpByLevel"] = T(
    "セーブの読み込み時とレベルアップのたびに、到達しているレベルの中で最も高い難易度がゲームの難易度になります ― 設定メニューで変更するのと同じ変更なので、再生設定もそれに追従します。0 はこのルールで選ばれないという意味です。オフ: ゲームの難易度は自分で設定します。",
    "세이브를 불러올 때와 레벨업할 때마다, 도달한 레벨 중 가장 높은 난이도가 게임의 난이도가 됩니다 ― 설정 메뉴에서 바꾸는 것과 같은 변경이므로 재생 설정도 따라갑니다. 0은 이 규칙으로 선택되지 않는다는 뜻입니다. 끔: 게임 난이도는 직접 설정합니다.",
    "在读取存档时以及每次升级时，你已达到其等级要求的最高难度会成为游戏难度 ― 这与在设置菜单中更改是同一种改动，因此回复设定也会随之切换。0 表示此规则永不选择该难度。关闭：游戏难度由你自己设定。",
    "При загрузке сохранения и при каждом повышении уровня сложностью игры становится наивысшая сложность, уровня которой вы достигли — то же изменение, что и в меню настроек, поэтому набор регенерации следует за ним. 0 — эта сложность никогда не выбирается правилом. Выкл.: сложность игры задаёте вы сами.",
    "Beim Laden eines Spielstands und bei jedem Stufenaufstieg wird die höchste Schwierigkeit, deren Stufe du erreicht hast, zur Schwierigkeit des Spiels - dieselbe Änderung, die das Einstellungsmenü vornimmt, also folgt der Regenerationssatz mit. 0 = diese Schwierigkeit wird von dieser Regel nie gewählt. Aus: Die Schwierigkeit bestimmst du selbst.",
    "Au chargement d'une sauvegarde et à chaque montée de niveau, la difficulté la plus élevée dont vous avez atteint le niveau devient celle du jeu - le même changement que dans le menu Paramètres, donc le jeu de régénération suit. 0 = cette difficulté n'est jamais choisie par cette règle. Désactivé : la difficulté du jeu vous appartient.",
    "Al cargar una partida y en cada subida de nivel, la dificultad más alta cuyo nivel hayas alcanzado pasa a ser la del juego: el mismo cambio que hace el menú de Ajustes, así que el conjunto de regeneración lo sigue. 0 = esa dificultad nunca la elige esta regla. Desactivado: la dificultad del juego es cosa tuya.",
    "Al caricamento di un salvataggio e a ogni passaggio di livello, la difficoltà più alta di cui hai raggiunto il livello diventa quella del gioco - lo stesso cambiamento che fa il menu Impostazioni, quindi il set di rigenerazione lo segue. 0 = quella difficoltà non viene mai scelta da questa regola. Disattivo: la difficoltà del gioco la scegli tu.",
    "Przy wczytaniu zapisu i przy każdym awansie na poziom najwyższy poziom trudności, którego poziom osiągnąłeś, staje się trudnością gry - ta sama zmiana, którą wykonuje menu Ustawień, więc zestaw regeneracji podąża za nią. 0 = ten poziom trudności nigdy nie jest wybierany przez tę regułę. Wyłączone: trudność gry ustawiasz sam.",
    "Při načtení uložené hry a při každém postupu na úroveň se obtížností hry stane nejvyšší obtížnost, jejíž úrovně jsi dosáhl - stejná změna, jakou provádí nabídka Nastavení, takže sada regenerace ji následuje. 0 = tuto obtížnost toto pravidlo nikdy nezvolí. Vypnuto: obtížnost hry si nastavuješ sám.")
TRANSLATIONS["CDUI_LevelArrow"] = T(
    "レベル %d -> %s", "레벨 %d -> %s", "等级 %d -> %s", "Уровень %d -> %s", "Stufe %d -> %s",
    "Niveau %d -> %s", "Nivel %d -> %s", "Livello %d -> %s", "Poziom %d -> %s", "Úroveň %d -> %s")
TRANSLATIONS["CDUI_NoRow"] = T(
    "該当する行なし", "해당하는 행 없음", "无适用行", "нет подходящей строки", "keine Zeile trifft zu",
    "aucune ligne ne s'applique", "ninguna fila aplica", "nessuna riga si applica",
    "żaden wiersz nie pasuje", "žádný řádek neplatí")
TRANSLATIONS["CDUI_FromLevel"] = T(
    "%s はレベル", "%s 시작 레벨", "%s 起始等级", "%s с уровня", "%s ab Stufe",
    "%s à partir du niveau", "%s desde el nivel", "%s dal livello", "%s od poziomu", "%s od úrovně")
TRANSLATIONS["CDUI_HelpLevelTable"] = T(
    "既定値は Blade and Blunt の区切りです: 10 レベルごとに難易度が一段階上がります。",
    "기본값은 Blade and Blunt의 기준입니다: 10레벨마다 난이도가 한 단계씩 올라갑니다.",
    "默认值取自 Blade and Blunt 的节点：每十级提升一个难度档。",
    "По умолчанию — вехи Blade and Blunt: один уровень сложности на каждые десять уровней.",
    "Die Standardwerte sind die Meilensteine von Blade and Blunt: eine Schwierigkeitsstufe je zehn Stufen.",
    "Les valeurs par défaut sont les jalons de Blade and Blunt : un cran de difficulté tous les dix niveaux.",
    "Los valores por defecto son los hitos de Blade and Blunt: un escalón de dificultad cada diez niveles.",
    "I valori predefiniti sono i traguardi di Blade and Blunt: un gradino di difficoltà ogni dieci livelli.",
    "Wartości domyślne to progi Blade and Blunt: jeden stopień trudności na dziesięć poziomów.",
    "Výchozí hodnoty jsou milníky Blade and Blunt: jeden stupeň obtížnosti na deset úrovní.")
TRANSLATIONS["CDUI_NowHeader"] = T(
    "現在ゲームが使っている値", "지금 게임이 사용 중인 값", "游戏当前使用的数值",
    "Что игра использует прямо сейчас", "Was das Spiel gerade verwendet",
    "Ce que le jeu utilise en ce moment", "Lo que el juego está usando ahora mismo",
    "Cosa sta usando il gioco in questo momento", "Czego gra używa w tej chwili",
    "Co hra právě teď používá")
TRANSLATIONS["CDUI_DamageAt"] = T(
    "%s でのダメージ: 被ダメージ x%.2f、与ダメージ x%.2f（読み込み時は x%.2f / x%.2f）",
    "%s의 피해: 받는 피해 x%.2f, 주는 피해 x%.2f (불러온 값 x%.2f / x%.2f)",
    "%s 难度的伤害：受到 x%.2f，造成 x%.2f（载入时为 x%.2f / x%.2f）",
    "Урон на сложности «%s»: x%.2f по вам, x%.2f от вас (при загрузке x%.2f / x%.2f)",
    "Schaden bei %s: x%.2f auf dich, x%.2f von dir (geladen mit x%.2f / x%.2f)",
    "Dégâts en %s : x%.2f sur vous, x%.2f par vous (chargé avec x%.2f / x%.2f)",
    "Daño en %s: x%.2f hacia ti, x%.2f por ti (cargado con x%.2f / x%.2f)",
    "Danni a %s: x%.2f su di te, x%.2f da te (caricato con x%.2f / x%.2f)",
    "Obrażenia na poziomie %s: x%.2f na ciebie, x%.2f od ciebie (wczytano z x%.2f / x%.2f)",
    "Poškození na obtížnosti %s: x%.2f na tebe, x%.2f od tebe (načteno s x%.2f / x%.2f)")
TRANSLATIONS["CDUI_NoCharacter"] = T(
    "キャラクターが読み込まれていません。", "불러온 캐릭터가 없습니다.", "尚未载入角色。",
    "Персонаж не загружен.", "Kein Charakter geladen.", "Aucun personnage chargé.",
    "No hay ningún personaje cargado.", "Nessun personaggio caricato.",
    "Nie wczytano żadnej postaci.", "Není načtena žádná postava.")
TRANSLATIONS["CDUI_NotEnabled"] = T(
    "無効です ― 何も書き込まれません。上の値はゲームが読み込んだままのものです。",
    "사용 중이 아닙니다 ― 아무것도 기록되지 않으며, 위의 값은 게임이 불러온 그대로입니다.",
    "未启用 ― 不会写入任何内容；上面的数值就是游戏载入时的数值。",
    "Не включено — ничего не записывается; значения выше — те, с которыми загрузилась игра.",
    "Nicht aktiviert - es wird nichts geschrieben; die Werte oben sind die, mit denen das Spiel geladen hat.",
    "Non activé - rien n'est écrit ; les valeurs ci-dessus sont celles avec lesquelles le jeu s'est chargé.",
    "No activado: no se escribe nada; los valores de arriba son con los que cargó el juego.",
    "Non attivo - non viene scritto nulla; i valori sopra sono quelli con cui il gioco si è caricato.",
    "Nie włączono - nic nie jest zapisywane; wartości powyżej to te, z którymi wczytała się gra.",
    "Nezapnuto - nic se nezapisuje; hodnoty výše jsou ty, se kterými se hra načetla.")

# --- the regeneration page ---------------------------------------------------------------------
TRANSLATIONS["CDUI_RegenHeader"] = T(
    "再生", "재생", "回复", "Восстановление", "Regeneration",
    "Régénération", "Regeneración", "Rigenerazione", "Regeneracja", "Regenerace")
TRANSLATIONS["CDUI_HelpRegenEnabled"] = T(
    "オフにすると、この MOD が最初に読み込んだときに記録した本当のバニラ値へ以下のすべての設定を戻します ― 単に「触るのをやめる」だけではありません。",
    "끄면 이 모드가 처음 불러왔을 때 기록해 둔 진짜 바닐라 값으로 아래의 모든 설정을 되돌립니다 ― 단순히 \"건드리지 않는다\"가 아닙니다.",
    "关闭会把下面每一项设置恢复为本模组首次载入时记录的真实原版数值 ― 而不仅仅是“不再改动它们”。",
    "Выключение возвращает каждую настройку ниже к настоящему ванильному значению, которое мод записал при первой загрузке, — а не просто «перестаёт их трогать».",
    "Aus setzt jede Einstellung unten auf den echten Vanilla-Wert zurück, den diese Mod beim ersten Laden erfasst hat - nicht bloß \"nicht mehr anfassen\".",
    "Désactivé remet chaque réglage ci-dessous à la vraie valeur vanilla que ce mod a relevée au premier chargement - pas seulement \"on n'y touche plus\".",
    "Desactivado devuelve cada ajuste de abajo al valor vanilla real que este mod capturó la primera vez que cargó, no solo \"dejar de tocarlos\".",
    "Disattivo riporta ogni impostazione qui sotto al vero valore vanilla che questa mod ha registrato al primo caricamento - non solo \"smettere di toccarli\".",
    "Wyłączenie przywraca każde ustawienie poniżej do prawdziwej podstawowej wartości, którą ta modyfikacja zapisała przy pierwszym wczytaniu - a nie tylko \"przestaje ich dotykać\".",
    "Vypnutí vrátí každé nastavení níže na skutečnou základní hodnotu, kterou si tento mod zaznamenal při prvním načtení - ne jen \"přestat se jich dotýkat\".")
TRANSLATIONS["CDUI_RegenIntro"] = T(
    "バニラには難易度ごとの再生設定がありません ― この MOD がそれを追加します。以下の各難易度は独自の戦闘中の回復速度と遅延を保持し、ゲーム本来のメニューで難易度を切り替えると、このページを開かなくてもその場で適用される組が切り替わります。",
    "바닐라에는 난이도별 재생이 없습니다 ― 이 모드가 추가합니다. 아래의 각 난이도는 고유한 전투 중 회복 속도와 지연을 가지며, 게임 자체 메뉴에서 난이도를 바꾸면 이 페이지를 열지 않아도 적용되는 세트가 즉시 전환됩니다.",
    "原版没有按难度区分的回复 ― 本模组补上了这一点。下面每个难度都保有自己的战斗回复速率与延迟；在游戏自身的菜单中切换难度即会实时切换所应用的那一组，无需打开本页面。",
    "В ванилле нет восстановления по сложностям — этот мод его добавляет. У каждой сложности ниже свои боевые скорости и задержки; смена сложности в собственном меню игры переключает применяемый набор на лету, и открывать эту страницу не нужно.",
    "Vanilla kennt keine Regeneration je Schwierigkeit - diese Mod fügt sie hinzu. Jede Schwierigkeit unten hat eigene Kampfraten und Verzögerungen; ein Schwierigkeitswechsel im Menü des Spiels schaltet den geltenden Satz live um, ohne diese Seite zu öffnen.",
    "Vanilla n'a pas de régénération par difficulté - ce mod l'ajoute. Chaque difficulté ci-dessous garde ses propres taux de combat et délais ; changer de difficulté dans le menu du jeu bascule le jeu de valeurs appliqué, en direct, sans ouvrir cette page.",
    "Vanilla no tiene regeneración por dificultad: este mod la añade. Cada dificultad de abajo guarda sus propias tasas de combate y demoras; cambiar de dificultad en el menú del propio juego cambia el conjunto aplicado, en vivo, sin abrir esta página.",
    "Vanilla non ha rigenerazione per difficoltà - questa mod la aggiunge. Ogni difficoltà qui sotto conserva le proprie velocità di combattimento e i propri ritardi; cambiare difficoltà nel menu del gioco cambia il set applicato, dal vivo, senza aprire questa pagina.",
    "Podstawowa gra nie ma regeneracji zależnej od trudności - ta modyfikacja ją dodaje. Każdy poziom trudności poniżej ma własne tempa i opóźnienia w walce; zmiana trudności we własnym menu gry przełącza stosowany zestaw na żywo, bez otwierania tej strony.",
    "Základní hra nemá regeneraci podle obtížnosti - tento mod ji přidává. Každá obtížnost níže si drží vlastní bojová tempa a prodlevy; změna obtížnosti ve vlastní nabídce hry přepne použitou sadu naživo, bez otevírání této stránky.")
TRANSLATIONS["CDUI_Editing"] = T(
    "編集中", "편집 중", "正在编辑", "Редактируется", "Bearbeitet",
    "Modification", "Editando", "In modifica", "Edytowane", "Upravuje se")
TRANSLATIONS["CDUI_HelpEditing"] = T(
    "下のスライダーが表示・編集している難易度です。",
    "아래 슬라이더가 보여 주고 편집하는 난이도입니다.",
    "下面的滑块所显示并编辑的是哪个难度的数值。",
    "Значения какой сложности показывают и меняют слайдеры ниже.",
    "Welche Schwierigkeit die Regler unten anzeigen und bearbeiten.",
    "La difficulté dont les curseurs ci-dessous affichent et modifient les valeurs.",
    "De qué dificultad muestran y editan los valores los deslizadores de abajo.",
    "Di quale difficoltà i cursori qui sotto mostrano e modificano i valori.",
    "Wartości którego poziomu trudności pokazują i edytują suwaki poniżej.",
    "Hodnoty které obtížnosti posuvníky níže ukazují a upravují.")
TRANSLATIONS["CDUI_CurrentDifficulty"] = T(
    "現在の難易度: %s。%s の値が適用されています。",
    "현재 난이도: %s. %s의 값이 적용 중입니다.",
    "当前难度：%s。正在使用 %s 的数值。",
    "Текущая сложность: %s. Действуют значения «%s».",
    "Aktuelle Schwierigkeit: %s. Die Werte von %s sind aktiv.",
    "Difficulté actuelle : %s. Les valeurs de %s sont actives.",
    "Dificultad actual: %s. Están activos los valores de %s.",
    "Difficoltà attuale: %s. Sono attivi i valori di %s.",
    "Obecny poziom trudności: %s. Aktywne są wartości poziomu %s.",
    "Aktuální obtížnost: %s. Aktivní jsou hodnoty obtížnosti %s.")
TRANSLATIONS["CDUI_CurrentNotApplied"] = T(
    "現在の難易度: まだ適用されていません。",
    "현재 난이도: 아직 적용되지 않았습니다.",
    "当前难度：尚未应用。",
    "Текущая сложность: ещё не применена.",
    "Aktuelle Schwierigkeit: noch nicht angewendet.",
    "Difficulté actuelle : pas encore appliquée.",
    "Dificultad actual: aún no aplicada.",
    "Difficoltà attuale: non ancora applicata.",
    "Obecny poziom trudności: jeszcze nie zastosowano.",
    "Aktuální obtížnost: zatím nepoužita.")
TRANSLATIONS["CDUI_CopyToAll"] = T(
    "この組をすべての難易度にコピー",
    "이 세트를 모든 난이도에 복사",
    "把这一组复制到所有难度",
    "Скопировать этот набор во все сложности",
    "Diesen Satz auf jede Schwierigkeit kopieren",
    "Copier ce jeu vers toutes les difficultés",
    "Copiar este conjunto a todas las dificultades",
    "Copia questo set su ogni difficoltà",
    "Skopiuj ten zestaw do wszystkich poziomów trudności",
    "Zkopírovat tuto sadu na všechny obtížnosti")
TRANSLATIONS["CDUI_StatusCopiedAll"] = T(
    "すべての難易度にコピーしました。保存するには「保存」を押してください。",
    "모든 난이도에 복사했습니다. 유지하려면 저장을 누르세요.",
    "已复制到所有难度。按“保存”以保留。",
    "Скопировано во все сложности. Нажмите «Сохранить», чтобы оставить.",
    "Auf jede Schwierigkeit kopiert. Drücke Speichern, um es zu behalten.",
    "Copié vers toutes les difficultés. Appuyez sur Enregistrer pour conserver.",
    "Copiado a todas las dificultades. Pulsa Guardar para conservarlo.",
    "Copiato su ogni difficoltà. Premi Salva per mantenerlo.",
    "Skopiowano do wszystkich poziomów trudności. Naciśnij Zapisz, aby zachować.",
    "Zkopírováno na všechny obtížnosti. Stiskni Uložit, aby to zůstalo.")
TRANSLATIONS["CDUI_HelpCopyToAll"] = T(
    "現在編集している組で、他のすべての難易度の戦闘中の回復速度と遅延を上書きします。",
    "현재 편집 중인 세트로 다른 모든 난이도의 전투 회복 속도와 지연을 덮어씁니다.",
    "用你当前编辑的这一组覆盖其他所有难度的战斗回复速率与延迟。",
    "Перезаписывает боевые скорости и задержки всех ОСТАЛЬНЫХ сложностей набором, который вы сейчас правите.",
    "Überschreibt die Kampfraten und Verzögerungen jeder ANDEREN Schwierigkeit mit dem Satz, den du gerade bearbeitest.",
    "Écrase les taux de combat et les délais de toutes les AUTRES difficultés avec le jeu que vous modifiez.",
    "Sobrescribe las tasas de combate y las demoras de todas las DEMÁS dificultades con el conjunto que estás editando.",
    "Sovrascrive le velocità di combattimento e i ritardi di ogni ALTRA difficoltà con il set che stai modificando.",
    "Nadpisuje tempa i opóźnienia walki wszystkich POZOSTAŁYCH poziomów trudności zestawem, który właśnie edytujesz.",
    "Přepíše bojová tempa a prodlevy všech OSTATNÍCH obtížností sadou, kterou právě upravuješ.")
TRANSLATIONS["CDUI_CopyFrom"] = T(
    "コピー元", "복사해 오기", "从此处复制", "Скопировать из", "Kopieren von",
    "Copier depuis", "Copiar desde", "Copia da", "Kopiuj z", "Zkopírovat z")
TRANSLATIONS["CDUI_StatusCopied"] = T(
    "コピーしました。保存するには「保存」を押してください。",
    "복사했습니다. 유지하려면 저장을 누르세요.",
    "已复制。按“保存”以保留。",
    "Скопировано. Нажмите «Сохранить», чтобы оставить.",
    "Kopiert. Drücke Speichern, um es zu behalten.",
    "Copié. Appuyez sur Enregistrer pour conserver.",
    "Copiado. Pulsa Guardar para conservarlo.",
    "Copiato. Premi Salva per mantenerlo.",
    "Skopiowano. Naciśnij Zapisz, aby zachować.",
    "Zkopírováno. Stiskni Uložit, aby to zůstalo.")
TRANSLATIONS["CDUI_HelpCopyFrom"] = T(
    "編集中の難易度を、選んだ難易度の現在の値から始めます。",
    "편집 중인 난이도를 선택한 난이도의 현재 값에서 시작합니다.",
    "让你正在编辑的难度以所选难度的当前数值为起点。",
    "Начинает сложность, которую вы правите, с текущих значений выбранной сложности.",
    "Startet die Schwierigkeit, die du bearbeitest, mit den aktuellen Werten der gewählten Schwierigkeit.",
    "Fait démarrer la difficulté que vous modifiez à partir des valeurs actuelles de la difficulté choisie.",
    "Hace que la dificultad que editas parta de los valores actuales de la dificultad elegida.",
    "Fa partire la difficoltà che stai modificando dai valori attuali della difficoltà scelta.",
    "Sprawia, że edytowany poziom trudności startuje z bieżących wartości wybranego poziomu.",
    "Nastaví upravovanou obtížnost na aktuální hodnoty zvolené obtížnosti.")
TRANSLATIONS["CDUI_InCombat"] = T(
    "戦闘中", "전투 중", "战斗中", "В бою", "Im Kampf",
    "En combat", "En combate", "In combattimento", "W walce", "V boji")
TRANSLATIONS["CDUI_HealthRegenRate"] = T(
    "体力の回復速度", "체력 재생 속도", "生命回复速率", "Скорость восстановления здоровья",
    "Gesundheitsregenerationsrate", "Taux de régénération de santé", "Tasa de regeneración de salud",
    "Velocità di rigenerazione della salute", "Tempo regeneracji zdrowia", "Rychlost regenerace zdraví")
TRANSLATIONS["CDUI_MagickaRegenRate"] = T(
    "マジカの回復速度", "매지카 재생 속도", "法力回复速率", "Скорость восстановления магии",
    "Magickaregenerationsrate", "Taux de régénération de magie", "Tasa de regeneración de magia",
    "Velocità di rigenerazione della magicka", "Tempo regeneracji magii", "Rychlost regenerace magicky")
TRANSLATIONS["CDUI_StaminaRegenRate"] = T(
    "スタミナの回復速度", "스태미나 재생 속도", "耐力回复速率", "Скорость восстановления запаса сил",
    "Ausdauerregenerationsrate", "Taux de régénération de vigueur", "Tasa de regeneración de aguante",
    "Velocità di rigenerazione del vigore", "Tempo regeneracji kondycji", "Rychlost regenerace výdrže")
TRANSLATIONS["CDUI_AfterDamage"] = T(
    "被弾後", "피해를 입은 뒤", "受到伤害之后", "После урона", "Nach Schaden",
    "Après des dégâts", "Tras recibir daño", "Dopo i danni", "Po otrzymaniu obrażeń", "Po utrpění poškození")
TRANSLATIONS["CDUI_HealthRegenDelay"] = T(
    "体力の回復遅延（秒）", "체력 재생 지연(초)", "生命回复延迟（秒）", "Задержка восстановления здоровья (с)",
    "Verzögerung der Gesundheitsregeneration (s)", "Délai de régénération de santé (s)",
    "Demora de regeneración de salud (s)", "Ritardo di rigenerazione della salute (s)",
    "Opóźnienie regeneracji zdrowia (s)", "Prodleva regenerace zdraví (s)")
TRANSLATIONS["CDUI_MagickaRegenDelay"] = T(
    "マジカの回復遅延（秒）", "매지카 재생 지연(초)", "法力回复延迟（秒）", "Задержка восстановления магии (с)",
    "Verzögerung der Magickaregeneration (s)", "Délai de régénération de magie (s)",
    "Demora de regeneración de magia (s)", "Ritardo di rigenerazione della magicka (s)",
    "Opóźnienie regeneracji magii (s)", "Prodleva regenerace magicky (s)")
TRANSLATIONS["CDUI_StaminaRegenDelay"] = T(
    "スタミナの回復遅延（秒）", "스태미나 재생 지연(초)", "耐力回复延迟（秒）", "Задержка восстановления запаса сил (с)",
    "Verzögerung der Ausdauerregeneration (s)", "Délai de régénération de vigueur (s)",
    "Demora de regeneración de aguante (s)", "Ritardo di rigenerazione del vigore (s)",
    "Opóźnienie regeneracji kondycji (s)", "Prodleva regenerace výdrže (s)")
TRANSLATIONS["CDUI_AVRegenDelay"] = T(
    "能力値全般の被弾後遅延（秒）", "능력치 전반의 피해 후 지연(초)", "通用属性受损延迟（秒）",
    "Общая задержка после урона по характеристике (с)", "Allgemeine Verzögerung bei geschädigtem Attribut (s)",
    "Délai générique après dégâts sur un attribut (s)", "Demora genérica de atributo dañado (s)",
    "Ritardo generico per attributo danneggiato (s)", "Ogólne opóźnienie uszkodzonego atrybutu (s)",
    "Obecná prodleva poškozené vlastnosti (s)")
TRANSLATIONS["CDUI_CeilingsNote"] = T(
    "遅延の上限 ― すべての難易度で共通の一つの値です（上の遅延をこの値より大きくしたときだけ意味があります）:",
    "지연 상한 ― 모든 난이도에 공통인 하나의 값입니다(위의 지연을 이 값보다 크게 올렸을 때만 의미가 있습니다):",
    "延迟上限 ― 所有难度共用的一个数值（只有当上面的延迟被调高到超过它时才起作用）：",
    "Потолки задержки — одно значение на все сложности (важны только когда задержка выше поднята выше них):",
    "Verzögerungsobergrenzen - ein Wert für jede Schwierigkeit (sie zählen erst, wenn eine Verzögerung oben darüber hinaus erhöht wird):",
    "Plafonds de délai - une seule valeur, toutes difficultés confondues (ils ne comptent qu'une fois un délai ci-dessus relevé au-delà) :",
    "Topes de demora: un solo valor para todas las dificultades (solo importan cuando una demora de arriba se sube por encima de ellos):",
    "Tetti di ritardo - un solo valore, per ogni difficoltà (contano solo quando un ritardo qui sopra viene alzato oltre):",
    "Górne granice opóźnienia - jedna wartość dla wszystkich poziomów trudności (liczą się dopiero, gdy opóźnienie powyżej przekroczy je):",
    "Stropy prodlevy - jedna hodnota pro všechny obtížnosti (záleží na nich, až když prodleva výše přesáhne je):")
TRANSLATIONS["CDUI_HealthCeiling"] = T(
    "体力の遅延上限（秒）", "체력 지연 상한(초)", "生命延迟上限（秒）", "Потолок задержки здоровья (с)",
    "Obergrenze der Gesundheitsverzögerung (s)", "Plafond du délai de santé (s)",
    "Tope de demora de salud (s)", "Tetto del ritardo della salute (s)",
    "Górna granica opóźnienia zdrowia (s)", "Strop prodlevy zdraví (s)")
TRANSLATIONS["CDUI_MagickaCeiling"] = T(
    "マジカの遅延上限（秒）", "매지카 지연 상한(초)", "法力延迟上限（秒）", "Потолок задержки магии (с)",
    "Obergrenze der Magickaverzögerung (s)", "Plafond du délai de magie (s)",
    "Tope de demora de magia (s)", "Tetto del ritardo della magicka (s)",
    "Górna granica opóźnienia magii (s)", "Strop prodlevy magicky (s)")
TRANSLATIONS["CDUI_StaminaCeiling"] = T(
    "スタミナの遅延上限（秒）", "스태미나 지연 상한(초)", "耐力延迟上限（秒）", "Потолок задержки запаса сил (с)",
    "Obergrenze der Ausdauerverzögerung (s)", "Plafond du délai de vigueur (s)",
    "Tope de demora de aguante (s)", "Tetto del ritardo del vigore (s)",
    "Górna granica opóźnienia kondycji (s)", "Strop prodlevy výdrže (s)")
TRANSLATIONS["CDUI_Situational"] = T(
    "状況別", "상황별", "特殊情况", "Особые случаи", "Situationsbedingt",
    "Situationnel", "Situacional", "Situazionale", "Sytuacyjne", "Situační")
TRANSLATIONS["CDUI_OutOfBreath"] = T(
    "息切れ時のスタミナ遅延（秒）", "숨이 찼을 때 스태미나 지연(초)", "力竭时的耐力延迟（秒）",
    "Задержка запаса сил при одышке (с)", "Ausdauerverzögerung bei Atemnot (s)",
    "Délai de vigueur à bout de souffle (s)", "Demora de aguante sin aliento (s)",
    "Ritardo del vigore col fiato corto (s)", "Opóźnienie kondycji przy zadyszce (s)",
    "Prodleva výdrže při vyčerpání dechu (s)")
TRANSLATIONS["CDUI_EssentialDown"] = T(
    "倒れた重要 NPC の回復速度", "쓰러진 필수 NPC의 재생 속도", "被击倒的关键 NPC 的回复速率",
    "Скорость восстановления сбитого важного NPC", "Regenerationsrate niedergeschlagener essenzieller NPCs",
    "Taux de régénération d'un PNJ essentiel à terre", "Tasa de regeneración de un PNJ esencial caído",
    "Velocità di rigenerazione di un PNG essenziale a terra", "Tempo regeneracji powalonego kluczowego NPC",
    "Rychlost regenerace sražené nezbytné NPC")

# --- debug and the buttons ---------------------------------------------------------------------
TRANSLATIONS["CDUI_DebugHeader"] = T(
    "デバッグ", "디버그", "调试", "Отладка", "Debug",
    "Débogage", "Depuración", "Debug", "Debugowanie", "Ladění")
TRANSLATIONS["CDUI_LogLevel"] = T(
    "ログレベル", "로그 수준", "日志级别", "Уровень журнала", "Protokollstufe",
    "Niveau de journal", "Nivel de registro", "Livello di log", "Poziom dziennika", "Úroveň logu")
TRANSLATIONS["CDUI_HelpLogLevel"] = T(
    "ログに即座に適用されます。既定では Trace で出荷されます ― CLAUDE.md の規則 31 を参照。",
    "로그에 즉시 적용됩니다. 기본값은 Trace로 출시됩니다 ― CLAUDE.md 규칙 31 참조.",
    "立即应用于日志。发布时默认为 Trace ― 见 CLAUDE.md 规则 31。",
    "Применяется к журналу немедленно. Поставляется с уровнем Trace по умолчанию — см. правило 31 в CLAUDE.md.",
    "Gilt sofort für das Protokoll. Wird standardmäßig mit Trace ausgeliefert - siehe CLAUDE.md Regel 31.",
    "S'applique immédiatement au journal. Livré en Trace par défaut - voir la règle 31 de CLAUDE.md.",
    "Se aplica al registro de inmediato. Se publica en Trace por defecto: véase la regla 31 de CLAUDE.md.",
    "Si applica subito al log. Viene distribuito con Trace come impostazione predefinita - vedi la regola 31 di CLAUDE.md.",
    "Stosuje się do dziennika natychmiast. Domyślnie wydawane z poziomem Trace - zob. regułę 31 w CLAUDE.md.",
    "Použije se na log okamžitě. Dodává se ve výchozím stavu s Trace - viz pravidlo 31 v CLAUDE.md.")
TRANSLATIONS["CDUI_SaveBtn"] = T(
    "保存", "저장", "保存", "Сохранить", "Speichern",
    "Enregistrer", "Guardar", "Salva", "Zapisz", "Uložit")
TRANSLATIONS["CDUI_StatusSaved"] = T(
    "設定を保存しました。", "설정을 저장했습니다.", "设置已保存。", "Настройки сохранены.",
    "Einstellungen gespeichert.", "Réglages enregistrés.", "Ajustes guardados.",
    "Impostazioni salvate.", "Ustawienia zapisane.", "Nastavení uloženo.")
TRANSLATIONS["CDUI_StatusSaveFail"] = T(
    "INI を保存できませんでした。理由はログを確認してください。",
    "INI를 저장할 수 없었습니다. 이유는 로그를 확인하세요.",
    "无法保存 INI。原因请见日志。",
    "Не удалось сохранить INI. Причина — в журнале.",
    "Die INI konnte nicht gespeichert werden. Warum, steht im Protokoll.",
    "Impossible d'enregistrer l'INI. Voir le journal pour la raison.",
    "No se pudo guardar el INI. Consulta el registro para saber por qué.",
    "Impossibile salvare l'INI. Vedi il log per il motivo.",
    "Nie udało się zapisać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI se nepodařilo uložit. Důvod najdeš v logu.")
TRANSLATIONS["CDUI_HelpSave"] = T(
    "上のすべての設定を INI に書き戻します。コメントや無関係なキーはそのまま残ります。",
    "위의 모든 설정을 INI에 다시 기록합니다. 주석과 관련 없는 키는 그대로 둡니다.",
    "把上面每一项设置写回 INI。注释和无关的键保持不变。",
    "Записывает все настройки выше обратно в INI. Комментарии и посторонние ключи не трогаются.",
    "Schreibt jede Einstellung oben zurück in die INI. Kommentare und fremde Schlüssel bleiben unangetastet.",
    "Réécrit tous les réglages ci-dessus dans l'INI. Les commentaires et les clés sans rapport sont laissés intacts.",
    "Escribe todos los ajustes de arriba de vuelta al INI. Los comentarios y las claves ajenas se dejan intactos.",
    "Riscrive ogni impostazione qui sopra nell'INI. Commenti e chiavi estranee restano intatti.",
    "Zapisuje wszystkie ustawienia powyżej z powrotem do pliku INI. Komentarze i niepowiązane klucze pozostają nietknięte.",
    "Zapíše všechna nastavení výše zpět do INI. Komentáře a nesouvisející klíče zůstanou beze změny.")
TRANSLATIONS["CDUI_ReloadBtn"] = T(
    "INI から再読み込み", "INI에서 다시 불러오기", "从 INI 重新载入", "Перечитать INI",
    "Aus INI neu laden", "Recharger depuis l'INI", "Recargar desde el INI",
    "Ricarica dall'INI", "Wczytaj ponownie z INI", "Znovu načíst z INI")
TRANSLATIONS["CDUI_StatusReloaded"] = T(
    "INI から設定を再読み込みしました。", "INI에서 설정을 다시 불러왔습니다.", "已从 INI 重新载入设置。",
    "Настройки перечитаны из INI.", "Einstellungen aus der INI neu geladen.",
    "Réglages rechargés depuis l'INI.", "Ajustes recargados desde el INI.",
    "Impostazioni ricaricate dall'INI.", "Ustawienia wczytane ponownie z pliku INI.",
    "Nastavení znovu načteno z INI.")
TRANSLATIONS["CDUI_StatusReloadFail"] = T(
    "INI を読み取れませんでした。理由はログを確認してください。",
    "INI를 읽을 수 없었습니다. 이유는 로그를 확인하세요.",
    "无法读取 INI。原因请见日志。",
    "Не удалось прочитать INI. Причина — в журнале.",
    "Die INI konnte nicht gelesen werden. Warum, steht im Protokoll.",
    "Impossible de lire l'INI. Voir le journal pour la raison.",
    "No se pudo leer el INI. Consulta el registro para saber por qué.",
    "Impossibile leggere l'INI. Vedi il log per il motivo.",
    "Nie udało się odczytać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI se nepodařilo přečíst. Důvod najdeš v logu.")
TRANSLATIONS["CDUI_HelpReload"] = T(
    "最後に保存してから加えた変更をすべて破棄し、INI をディスクから読み直して、すぐに適用します。",
    "마지막 저장 이후의 모든 변경을 버리고 INI를 디스크에서 다시 읽어 즉시 적용합니다.",
    "丢弃自上次保存以来的所有更改，从磁盘重新读取 INI，并立即应用。",
    "Отбрасывает все изменения, сделанные здесь после последнего сохранения, перечитывает INI с диска и применяет её сразу.",
    "Verwirft jede Änderung seit dem letzten Speichern, liest die INI erneut von der Festplatte und wendet sie sofort an.",
    "Annule toute modification faite ici depuis la dernière sauvegarde, relit l'INI depuis le disque et l'applique aussitôt.",
    "Descarta cualquier cambio hecho aquí desde el último guardado, relee el INI del disco y lo aplica de inmediato.",
    "Scarta ogni modifica fatta qui dall'ultimo salvataggio, rilegge l'INI dal disco e la applica subito.",
    "Odrzuca wszelkie zmiany wprowadzone od ostatniego zapisu, ponownie odczytuje plik INI z dysku i natychmiast go stosuje.",
    "Zahodí všechny změny od posledního uložení, znovu načte INI z disku a okamžitě ho použije.")
TRANSLATIONS["CDUI_RestoreBtn"] = T(
    "既定値に戻す", "기본값 복원", "恢复默认值", "Восстановить значения по умолчанию",
    "Standardwerte wiederherstellen", "Restaurer les valeurs par défaut", "Restaurar valores por defecto",
    "Ripristina i valori predefiniti", "Przywróć domyślne", "Obnovit výchozí")
TRANSLATIONS["CDUI_StatusRestored"] = T(
    "既定値に戻して適用しました。保持するには「保存」を押してください。",
    "기본값을 복원하고 적용했습니다. 유지하려면 저장을 누르세요.",
    "已恢复默认值并应用。按“保存”以保留。",
    "Значения по умолчанию восстановлены и применены. Нажмите «Сохранить», чтобы оставить их.",
    "Standardwerte wiederhergestellt und angewendet. Drücke Speichern, um sie zu behalten.",
    "Valeurs par défaut restaurées et appliquées. Appuyez sur Enregistrer pour les conserver.",
    "Valores por defecto restaurados y aplicados. Pulsa Guardar para conservarlos.",
    "Valori predefiniti ripristinati e applicati. Premi Salva per mantenerli.",
    "Przywrócono i zastosowano wartości domyślne. Naciśnij Zapisz, aby je zachować.",
    "Výchozí hodnoty obnoveny a použity. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CDUI_HelpRestore"] = T(
    "すべての設定を新規インストール時の値に戻し、すぐに適用します。「保存」を押すまで INI には何も書き込まれません。",
    "모든 설정을 새로 설치했을 때의 값으로 되돌리고 즉시 적용합니다. 저장을 누르기 전까지는 INI에 아무것도 기록되지 않습니다.",
    "把每一项设置恢复为全新安装时的数值并立即应用。在你按下“保存”之前不会写入 INI。",
    "Возвращает каждую настройку к значению при свежей установке и сразу применяет. В INI ничего не пишется, пока вы не нажмёте «Сохранить».",
    "Setzt jede Einstellung auf den Wert einer frischen Installation zurück und wendet ihn sofort an. In die INI wird nichts geschrieben, bis du Speichern drückst.",
    "Remet chaque réglage à sa valeur d'installation neuve et l'applique aussitôt. Rien n'est écrit dans l'INI tant que vous n'appuyez pas sur Enregistrer.",
    "Devuelve cada ajuste al valor de una instalación nueva y lo aplica de inmediato. No se escribe nada en el INI hasta que pulses Guardar.",
    "Riporta ogni impostazione al valore di un'installazione nuova e la applica subito. Nell'INI non viene scritto nulla finché non premi Salva.",
    "Przywraca każde ustawienie do wartości ze świeżej instalacji i natychmiast je stosuje. Do pliku INI nic nie jest zapisywane, dopóki nie naciśniesz Zapisz.",
    "Vrátí každé nastavení na hodnotu čerstvé instalace a hned je použije. Do INI se nic nezapíše, dokud nestiskneš Uložit.")
TRANSLATIONS["CDUI_SettingsIntro"] = T(
    "以下の変更は、ゲーム本来の難易度ダメージ倍率 ― バニラの難易度スライダーがレベルごとに設定するのと同じもの ― に即座に適用されます。次回も残すには別途「保存」を押してください。",
    "아래의 변경은 게임 자체의 난이도 피해 배율 ― 바닐라 난이도 슬라이더가 단계별로 설정하는 바로 그 값 ― 에 즉시 적용됩니다. 다음에도 유지하려면 따로 저장을 누르세요.",
    "下面的更改会立即作用于游戏自身的难度伤害倍率 ― 就是原版难度滑块按档位设定的那些。若要保留到下次，请另外按“保存”。",
    "Изменения ниже сразу применяются к собственным множителям урона по сложности — тем же, что ставит ванильный ползунок сложности на каждом уровне. Нажмите «Сохранить» отдельно, чтобы они остались на следующий раз.",
    "Änderungen unten wirken sofort auf die Schadensmultiplikatoren der Spielschwierigkeit - dieselben, die der Vanilla-Schwierigkeitsregler je Stufe setzt. Drücke zusätzlich Speichern, um sie fürs nächste Mal zu behalten.",
    "Les modifications ci-dessous s'appliquent immédiatement aux multiplicateurs de dégâts de difficulté du jeu - ceux-là mêmes que règle le curseur de difficulté vanilla, par cran. Appuyez séparément sur Enregistrer pour les conserver la prochaine fois.",
    "Los cambios de abajo se aplican de inmediato a los multiplicadores de daño de dificultad del propio juego: los mismos que fija el deslizador de dificultad vanilla, por escalón. Pulsa Guardar aparte para conservarlos la próxima vez.",
    "Le modifiche qui sotto si applicano subito ai moltiplicatori di danno della difficoltà del gioco - gli stessi che imposta il cursore di difficoltà vanilla, per gradino. Premi Salva a parte per mantenerle la prossima volta.",
    "Zmiany poniżej działają natychmiast na własne mnożniki obrażeń trudności w grze - te same, które ustawia podstawowy suwak trudności, dla każdego stopnia. Naciśnij osobno Zapisz, aby zachować je na następny raz.",
    "Změny níže se okamžitě projeví na vlastních násobitelích poškození podle obtížnosti - týchž, které nastavuje základní posuvník obtížnosti pro každý stupeň. Zvlášť stiskni Uložit, aby zůstaly i příště.")

# --- the log levels -----------------------------------------------------------------------------
TRANSLATIONS["CDUI_Log_Trace"] = T(
    "トレース", "추적", "追踪", "Трассировка", "Trace",
    "Trace", "Traza", "Traccia", "Śledzenie", "Trace")
TRANSLATIONS["CDUI_Log_Debug"] = T(
    "デバッグ", "디버그", "调试", "Отладка", "Debug",
    "Débogage", "Depuración", "Debug", "Debugowanie", "Ladění")
TRANSLATIONS["CDUI_Log_Info"] = T(
    "情報", "정보", "信息", "Информация", "Info",
    "Info", "Información", "Info", "Informacje", "Info")
TRANSLATIONS["CDUI_Log_Warning"] = T(
    "警告", "경고", "警告", "Предупреждение", "Warnung",
    "Avertissement", "Advertencia", "Avviso", "Ostrzeżenie", "Varování")
TRANSLATIONS["CDUI_Log_Error"] = T(
    "エラー", "오류", "错误", "Ошибка", "Fehler",
    "Erreur", "Error", "Errore", "Błąd", "Chyba")
TRANSLATIONS["CDUI_Log_Critical"] = T(
    "重大", "심각", "严重", "Критическая", "Kritisch",
    "Critique", "Crítico", "Critico", "Krytyczny", "Kritické")
TRANSLATIONS["CDUI_Log_Off"] = T(
    "オフ", "끄기", "关闭", "Выкл.", "Aus",
    "Désactivé", "Desactivado", "Disattivato", "Wyłączone", "Vypnuto")

# --- Skyrim's own six difficulty names ----------------------------------------------------------
TRANSLATIONS["CDUI_Diff_Novice"] = T(
    "初心者", "초보", "新手", "Новичок", "Novize",
    "Novice", "Novato", "Novizio", "Nowicjusz", "Nováček")
TRANSLATIONS["CDUI_Diff_Apprentice"] = T(
    "見習い", "수습", "学徒", "Ученик", "Lehrling",
    "Apprenti", "Aprendiz", "Apprendista", "Uczeń", "Učeň")
TRANSLATIONS["CDUI_Diff_Adept"] = T(
    "一人前", "숙련", "熟练", "Адепт", "Adept",
    "Adepte", "Adepto", "Adepto", "Adept", "Adept")
TRANSLATIONS["CDUI_Diff_Expert"] = T(
    "熟練者", "전문가", "专家", "Эксперт", "Experte",
    "Expert", "Experto", "Esperto", "Ekspert", "Expert")
TRANSLATIONS["CDUI_Diff_Master"] = T(
    "達人", "달인", "大师", "Мастер", "Meister",
    "Maître", "Maestro", "Maestro", "Mistrz", "Mistr")
TRANSLATIONS["CDUI_Diff_Legendary"] = T(
    "伝説", "전설", "传奇", "Легендарный", "Legendär",
    "Légendaire", "Legendario", "Leggendario", "Legendarny", "Legendární")


# ------------------------------------------------------------------------------------------------
# Writing the eleven files.
# ------------------------------------------------------------------------------------------------
def write_file(path, order, records):
    lines = []
    for key in order:
        lines.append("$" + key + "\t" + records[key].replace("\n", "\\n"))
    body = "\r\n".join(lines) + "\r\n"
    with io.open(path, "wb") as f:
        f.write(b"\xff\xfe")
        f.write(body.encode("utf-16-le"))


def main():
    order, english = read_keys()
    print("source/UI.cpp: {} keys".format(len(order)))

    missing = [k for k in order if k not in TRANSLATIONS]
    extra = [k for k in TRANSLATIONS if k not in english]
    if missing:
        raise SystemExit("no translations held for {} key(s): {}".format(len(missing), ", ".join(missing)))
    if extra:
        raise SystemExit("translations held for {} key(s) the source does not use: {}".format(len(extra), ", ".join(extra)))

    out_dir = os.path.join(REPO, "dist", "Interface", "Translations")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    for lang in LANGS:
        if lang == "english":
            records = english
        else:
            records = {k: TRANSLATIONS[k][lang] for k in order}
        path = os.path.join(out_dir, "{}_{}.txt".format(STEM, lang))
        write_file(path, order, records)
        print("  {:9s} {:3d} keys -> {}".format(lang, len(order), os.path.basename(path)))


if __name__ == "__main__":
    main()
