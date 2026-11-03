#!/usr/bin/env python3
"""OffTec PortAI — Menus exatamente como existem no original (portai.py / portai_i18n.py).

Este módulo reproduz a estrutura de menus e rótulos originais da assistente
OffTec PortAI, num novo projeto localizado no repositório AoM-Native-Port.
Não se trata de um build nem de execução automática; é apenas a reprodução
estrita das definições dos menus, preservando números, rótulos, prompts e
peças de ajuda textual do código original.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

# --- Línguas oficiais (última versão catalogada em portai_i18n.py) ---
LOCALES = ("pt-BR", "en", "es", "ar", "hi")
LOCALE_NAMES = {
    "pt-BR": "Português brasileiro",
    "en": "English",
    "es": "Español",
    "ar": "العربية",
    "hi": "हिन्दी",
}

# Catálogo completo, com pt/en/es/ar/hi, obtido verbatim de portai_i18n.py _ROWS.
_ROWS: dict[str, tuple[str, str, str, str]] = {
    "OFFTEC PORTAI": ("OFFTEC PORTAI", "OFFTEC PORTAI", "أوفتيك بورت آي", "ऑफटेक पोर्टएआई"),
    "Análise estática": ("Static analysis", "Análisis estático", "تحليل ساكن", "स्थिर विश्लेषण"),
    "cópias isoladas": ("isolated workspaces", "copias aisladas", "مساحات عمل معزولة", "अलग कार्यक्षेत्र"),
    "integração com Codex CLI": ("Codex CLI integration", "integración con Codex CLI", "تكامل Codex CLI", "Codex CLI एकीकरण"),
    "Encontrar e analisar projetos": ("Find and analyze projects", "Buscar y analizar proyectos", "العثور على المشاريع وتحليلها", "प्रोजेक्ट खोजें और विश्लेषण करें"),
    "Listar projetos detectados e escolher pelo número": ("List detected projects and choose by number", "Listar proyectos detectados y elegir por número", "عرض المشاريع المكتشفة والاختيار بالرقم", "पहचाने गए प्रोजेक्ट सूचीबद्ध करें और संख्या से चुनें"),
    "Caminho completo": ("Full path", "Ruta completa", "المسار الكامل", "पूरा पथ"),
    "01 — PROJETOS": ("01 — PROJECTS", "01 — PROYECTOS", "01 — المشاريع", "01 — प्रोजेक्ट"),
    "02 — APK BUILD CENTER": ("02 — APK BUILD CENTER", "02 — CENTRO DE COMPILACIÓN APK", "02 — مركز بناء APK", "02 — APK बिल्ड सेंटर"),
    "03 — TRADUÇÃO": ("03 — TRANSLATION", "03 — TRADUCCIÓN", "03 — الترجمة", "03 — अनुवाद"),
    "04 — GAME PORTABILITY": ("04 — GAME PORTABILITY", "04 — PORTABILIDAD DE JUEGOS", "04 — قابلية نقل الألعاب", "04 — गेम पोर्टेबिलिटी"),
    "05 — CONFIGURAÇÕES": ("05 — SETTINGS", "05 — CONFIGURACIÓN", "05 — الإعدادات", "05 — सेटिंग्स"),
    "Histórico e relatórios": ("History and reports", "Historial e informes", "السجل والتقارير", "इतिहास और रिपोर्ट"),
    "APK Build Center": ("APK Build Center", "Centro de compilación APK", "مركز بناء APK", "APK बिल्ड सेंटर"),
    "Reconstruir APK com Apktool": ("Rebuild APK with Apktool", "Reconstruir APK con Apktool", "إعادة بناء APK باستخدام Apktool", "Apktool से APK फिर बनाएँ"),
    "Assinar APK com sua keystore": ("Sign APK with your keystore", "Firmar APK con su keystore", "توقيع APK باستخدام keystore الخاصة بك", "अपनी keystore से APK साइन करें"),
    "Verificar integridade e assinatura": ("Check integrity and signature", "Verificar integridad y firma", "التحقق من السلامة والتوقيع", "इंटीग्रिटी और हस्ताक्षर जाँचें"),
    "Alinhar APK com zipalign": ("Align APK with zipalign", "Alinear APK con zipalign", "محاذاة APK باستخدام zipalign", "zipalign से APK अलाइन करें"),
    "Histórico de builds": ("Build history", "Historial de compilaciones", "سجل عمليات البناء", "बिल्ड इतिहास"),
    "Verificar qualidade das traduções": ("Check translation quality", "Verificar la calidad de las traducciones", "التحقق من جودة الترجمات", "अनुवाद की गुणवत्ता जाँचें"),
    "Analisar jogos": ("Analyze games", "Analizar juegos", "تحليل الألعاب", "गेम का विश्लेषण करें"),
    "Planejar adaptação": ("Plan adaptation", "Planificar adaptación", "تخطيط التكييف", "अनुकूलन की योजना बनाएँ"),
    "Reconstruir com Codex": ("Rebuild with Codex", "Reconstruir con Codex", "إعادة البناء باستخدام Codex", "Codex से पुनर्निर्माण करें"),
    "Ajuda e informações": ("Help and information", "Ayuda e información", "المساعدة والمعلومات", "सहा"),
    "Manual de ajuda": ("General manual", "Manual general", "دليل المستخدم العام", "सामान्य मैनुअल"),
    "Roadmap futuro": ("Future roadmap", "Roadmap futuro", "المسار المستقبلي", "भविष्य रोडमैप"),
    "Fóruns e comunidade": ("Forums and community", "Foros y comunidad", "منتديات المجتمع", "सामुदायिक मंच"),
    "Relatórios de tradução": ("Translation reports", "Informes de traducción", "تقارير الترجمة", "अनुवाद रिपोर्ट"),
    "Localizar fontes e calendário": ("Locate fonts and calendar", "Localizar fuentes y calendario", "Locate fonts and calendar", "फ़ॉन्ट और कैलेंडर खोजें"),
    "Padrão offline-review": ("offline-review default", "modo offline-review por defecto", "أسلوب offline-review افتراضي", "offline-review डिफ़ॉल्ट"),
}

# Origem: https://github.com/Mcsd7/OffTec-PortAI (versão 0.3.0).
# Conversão centralizada para as traduções desses rótulos.
_CATALOGS: dict[str, dict[str, str]] = {}
for index, locale in enumerate(("en", "es", "ar", "hi")):
    _CATALOGS[locale] = {
        source: translations[index]
        for source, translations in _ROWS.items()
    }

_locale = "pt-BR"
CONFIG_PATH = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))) / "offtec-portai" / "settings.json"


def load_locale() -> str:
    global _locale
    try:
        data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        choice = data.get("interface_language", "pt-BR")
        _locale = choice if choice in LOCALES else "pt-BR"
    except (OSError, ValueError, AttributeError):
        _locale = "pt-BR"
    return _locale


def save_locale(locale: str) -> None:
    global _locale
    if locale not in LOCALES:
        raise ValueError(f"Unsupported interface language: {locale}")
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    payload: dict[str, Any] = {}
    try:
        current = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        if isinstance(current, dict):
            payload.update(current)
    except (OSError, ValueError):
        pass
    payload["interface_language"] = locale
    temp_path = CONFIG_PATH.with_suffix(CONFIG_PATH.suffix + ".tmp")
    temp_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temp_path.replace(CONFIG_PATH)
    _locale = locale


def get_locale() -> str:
    return _locale


def locale_choices() -> list[str]:
    return [f"{number}. {LOCALE_NAMES[locale]} [{locale}]" for number, locale in enumerate(LOCALES, 1)]


def locale_from_choice(choice: str) -> str:
    number = int(choice.split(".", 1)[0])
    if number < 1 or number > len(LOCALES):
        raise ValueError("Invalid interface language selection")
    return LOCALES[number - 1]


load_locale()


def tr(message: str) -> str:
    target = _locale if _locale != "pt-BR" else None
    if target not in _CATALOGS or not message:
        return message
    catalog = _CATALOGS[target]
    for source in sorted(catalog, key=len, reverse=True):
        if source in message:
            translated = catalog[source]
            if target == "ar":
                translated = f"\u2068{translated}\u2069"
            message = message.replace(source, translated)
    return message


def ask_prompt(prompt: str, **kwargs: Any) -> str:
    from rich.prompt import Prompt
    return Prompt.ask(tr(prompt), **kwargs)


def ask_confirm(prompt: str, **kwargs: Any) -> bool:
    from rich.prompt import Confirm
    return Confirm.ask(tr(prompt), **kwargs)


# ---------------------------------------------------------------------------
# Dados dos menus — exatamente como no original portai.py
# ---------------------------------------------------------------------------

# Menu principal: setas de seleção numérica.
MAIN_MENU_ITEMS: list[tuple[str, str]] = [
    ("01 — PROJETOS", "Encontrar e analisar projetos"),
    ("02 — APK BUILD CENTER", "APK Build Center"),
    ("03 — TRADUÇÃO", "Tradução"),
    ("04 — GAME PORTABILITY", "Game Portability"),
    ("05 — CONFIGURAÇÕES", "Configurações"),
]

# Ícones secundários do menu principal.
HISTORY_MENU_ITEMS: list[tuple[str, str]] = [
    ("Histórico e relatórios", "History and reports"),
]

# Opções adicionais do menu principal.
OPTIONAL_MENU_ITEMS: list[tuple[str, str]] = [
    ("Reconstruir APK com Apktool", "Rebuild APK with Apktool"),
    ("Assinar APK com sua keystore", "Sign APK with your keystore"),
    ("Verificar integridade e assinatura", "Check integrity and signature"),
    ("Alinhar APK com zipalign", "Align APK with zipalign"),
]

# Ajuda textual dos ítens dos menus principais.
HELP_TEXT_KEYS: dict[str, str] = {
    "01 — PROJETOS": "Set up and inspect Windows projects before Android adaptation.",
    "02 — APK BUILD CENTER": "Rebuild, sign and validate APKs inside Termux.",
    "03 — TRADUÇÃO": "Translate project resources in JSON, XML, TXT, PO, RESX, INI and YAML.",
    "04 — GAME PORTABILITY": "Heuristic analysis of Unity/Unreal/Godot/GameMaker projects.",
    "05 — CONFIGURAÇÕES": "Choose interface language and manage saved settings.",
}

# Ajuda textual do menu Histórico e relatórios.
HELP_TEXT_KEYS_HISTORY: dict[str, str] = {
    "Histórico e relatórios": "List generated reports and inspect their paths and format.",
}

# Prompts e pedidos definidos pelas páginas dos menus.
SETTINGS_PROMPTS = {
    "Language": "Choose interface language:",
    "Reports": "Path to store generated reports:",
    "Logout": "Name to register at the logout counter:",
    "Reset": "Restore default settings and delete stored state?",
}


def show_menu(items: list[tuple[str, str]]) -> int:
    """Imprime um menu e retorna a escolha numérica ou 0 para sair."""
    print(tr("\nMenu principal"))
    print("-" * 60)
    for index, (title, label) in enumerate(items, start=1):
        print(f"{index}. {title} — {label}")
    print("0. Sair")
    choice = ask_prompt("Escolha uma opção", default="0")
    try:
        number = int(choice)
    except ValueError:
        return 0
    return number if 0 < number <= len(items) else 0


def help_texts() -> dict[str, str]:
    return {key: HELP_TEXT_KEYS.get(key, "") for key in tuple(HELP_TEXT_KEYS)} | {
        key: HELP_TEXT_KEYS_HISTORY.get(key, "") for key in tuple(HELP_TEXT_KEYS_HISTORY)
    }


def settings_menu() -> None:
    """Página de configurações reproduzida como no original."""
    print(tr("\nConfigurações"))
    print("-" * 60)
    LOCALE_CHOICES = locale_choices()
    for index, key in enumerate(LOCALE_CHOICES, start=1):
        print(f"{index}. {key}")
    print("0. Sair")
    choice = ask_prompt("Escolha uma opção", default="0")
    try:
        number = int(choice)
    except ValueError:
        return
    if number == 0:
        return
    if number == len(LOCALE_CHOICES) + 1:
        save_locale(get_locale())
        print(tr("[green]Idioma salvo.[/]"))
    elif 1 <= number <= len(LOCALE_CHOICES):
        save_locale(locale_from_choice(LOCALE_CHOICES[number - 1]))
        print(tr(f"[green]Idioma alterado para {LOCALE_CHOICES[number - 1]}.[/]\n"))
    else:
        print(tr("[yellow]Opção inválida.[/]\n"))


def history_menu() -> None:
    """Menu histórico republicado como no original."""
    print(tr("\nHistórico e relatórios"))
    print("-" * 60)
    print(tr(HELP_TEXT_KEYS["Histórico e relatórios"]))
    print("0. Sair")
    choice = ask_prompt("Escolha uma opção", default="0")
    print(tr("[red]Execução simulada:[/] nenhuma report foi gerado neste fluxo.[/]\n"))


def help_menu() -> None:
    """Menu de ajuda republicado como no original."""
    print(tr("\nAjuda e informações"))
    print("-" * 60)
    print(tr(HELP_TEXT_KEYS["Ajuda e informações"]))
    print("0. Sair")
    choice = ask_prompt("Escolha uma opção", default="0")
    if choice == "1":
        manual = ask_prompt("Manual de ajuda", default="manual.md")
        print(f"[green]Manual solicitado:[/] {manual}")
    elif choice == "2":
        roadmap = ask_prompt("Roadmap futuro", default="roadmap.md")
        print(f"[green]Roadmap solicitado:[/] {roadmap}")
    elif choice == "3":
        forum = ask_prompt("Fóruns e comunidade", default="https://discourse.example.com")
        print(f"[green]Fórum solicitado:[/] {forum}")
    print(tr("[red]Execução simulada:[/] nenhum conteúdo foi criado neste fluxo.[/]\n"))


def main_flow() -> None:
    """Fluxo principal exatamente como no original portai.main()."""
    while True:
        result = show_menu(MAIN_MENU_ITEMS)
        if result == 0:
            print(tr("[cyan]Saindo...[/]\n"))
            break
        elif result == 1:
            print(tr("[green]Página 'Encontrar e analisar projetos':[/] defina a pasta e analise projetos.\n"))
        elif result == 2:
            print(tr("[green]Página 'APK BUILD CENTER':[/] reconstrua, assine e valide APKs.\n"))
        elif result == 3:
            print(tr("[green]Página 'Tradução':[/] traduza recursos do projeto.\n"))
        elif result == 4:
            print(tr("[green]Página 'Game Portability':[/] heurística de jogos.\n"))
        elif result == 5:
            settings_menu()
        else:
            print(tr("[yellow]Opção inválida.[/]\n"))


if __name__ == "__main__":
    main_flow()
