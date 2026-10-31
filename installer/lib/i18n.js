// Interface texts in the five languages of the European disc.
// Static texts in index.html name their key with data-i18n (plain text) or data-i18n-html (text with markup).

export const LANGUAGES = [
  { code: 'en', name: 'English', flag: 'uk' },
  { code: 'es', name: 'Español', flag: 'spain' },
  { code: 'fr', name: 'Français', flag: 'france' },
  { code: 'de', name: 'Deutsch', flag: 'germany' },
  { code: 'it', name: 'Italiano', flag: 'italy' },
];

const SPHAIRA = '<a href="https://github.com/NaGaa95/sphaira/releases" target="_blank" rel="noopener">Sphaira</a>';
const RECOMP = '<a href="https://github.com/rob646464/luigis-mansion-recompiled" target="_blank" rel="noopener">Luigi\'s Mansion Recompiled</a>';
// The projects named in the credits.
const link = (url, text) => `<a href="${url}" target="_blank" rel="noopener">${text}</a>`;
const DUSKLIGHT = 'https://github.com/TwilitRealm/dusklight';
const AURORA = 'https://github.com/encounter/aurora';
const TP_ALEKS = 'https://github.com/Alexgg1014/The-Legend-of-Zelda-Twilight-Princess-Alek-s-Ultimate-NX-Edition';
const CHANSEY = 'https://github.com/ChanseyIsTheBest';
const FINCS = 'https://github.com/fincs';
const DEVKITPRO = 'https://devkitpro.org';
const DEKO3D = 'https://github.com/devkitPro/deko3d';
const UAM = 'https://github.com/devkitPro/uam';
const LIBNX = 'https://github.com/switchbrew/libnx';
const DOLPHIN = 'https://dolphin-emu.org';
const SWITCHWAKER = 'https://github.com/centollOS/SwitchWaker';
const SWITCHWAKERHD = 'https://github.com/centollOS/SwitchWakerHD';
const LMNX_REPO = 'https://github.com/StevensND/lm-nx';
const ISSUES = 'https://github.com/StevensND/lm-nx/issues';
const YOUTUBE = 'https://www.youtube.com/@StevensxD';
// French puts a non-breaking space before ":" and "?".
const NB = ' ';

const TEXTS = {
  en: {
    menuTitle: 'Menu',
    menuOpen: 'Open menu',
    menuClose: 'Close menu',
    menuLinks: 'Links',
    menuCredits: 'Credits',
    menuHelp: 'Help',
    menuInstaller: 'Installer',
    menuDisclaimer: 'Disclaimer',
    pageDisclaimer:
      `<ul><li><strong>LM-NX is a community-made port.</strong> It's based on the work of <strong>rob646464</strong> (${RECOMP}) ` +
      `adapted to Nintendo Switch.</li>` +
      `<li><strong>This port is free.</strong> Selling it isn't allowed. If you paid for it or for having it ` +
      `installed, you were scammed.</li>` +
      `<li><strong>Tools like AIO Switch Updater and similar and sites like NXMaster aren't affiliated with or endorsed by this ` +
      `project.</strong><br>They distribute the package with the disc images already included and sharing the game's disc images ` +
      `like that is illegal.<br><strong>This page never does that:</strong> to make the package, you must provide your own ` +
      `disc images, dumped from your own discs.</li>` +
      `<li><strong>Luigi's Mansion is a trademark of Nintendo.</strong> This project is <strong>not affiliated with Nintendo</strong>.</li></ul>` +
      `<p><strong>Official sources:</strong> only this page and the ${link(LMNX_REPO, 'lm-nx')} repository on GitHub. ` +
      `Any other download doesn't come from this project.</p>`,
    menuAbout: 'About',
    navHowto: 'How to use the installer',
    helpButton: 'Help',
    gameLanguage: `Game language`,
    gameLanguageHint: `Spanish, German, French and Italian need the European disc: the game then starts in the European version, in that language. Making lm-nx.zip again with a language replaces your settings.`,
    navRequirements: 'Requirements',
    navControls: 'Controls',
    navFaq: 'FAQ',
    navTrouble: 'Troubleshooting',
    navReport: 'Report a bug',
    navNews: 'Changelog',
    navMedia: 'Screenshots & videos',
    pageHowto:
      `<h3>Before you start</h3><ul>` +
      `<li>Use a computer with an up-to-date browser: the package is made on your computer, and it is big.</li>` +
      `<li>Your own copy of <strong>Luigi's Mansion (USA)</strong>, dumped from your disc, is <strong>required</strong>. The <strong>European</strong> disc (first release) is optional: it adds the Hidden Mansion and more languages.</li>` +
      `<li>Check the free space you need in <a href="#requirements">Requirements</a>.</li></ul>` +
      `<h3>1. Choose your discs</h3><p>Press <strong>Choose disc images</strong> and pick your <code>.iso</code>, <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> or <code>.ciso</code>. The page turns <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> and <code>.ciso</code> images into a full <code>.iso</code> while it makes the package. You can pick both discs at once, or one after the other. Each disc says <strong>found, ready to use</strong> when it is accepted. If a disc is not accepted, the message under it tells you why.</p>` +
      `<h3>2. Create the package</h3><p>Installing for the first time? Press <strong>Create lm-nx.zip</strong>. Already installed? Press <strong>Create lm-nx-update.zip</strong>: your discs and saves stay as they are.</p>` +
      `<p>The page downloads the package, checks it, compiles the shaders and, with the European disc, builds the Hidden Mansion. This can take a few minutes. <strong>Keep this tab open</strong> until it says <strong>Done</strong>. If the browser asks where to save the file, choose a folder.</p>` +
      `<h3>3. Copy it to the Switch</h3><p>Extract the <code>.zip</code> and copy the <code>LM-NX</code> folder inside it into <code>sdmc:/switch/</code> on your SD card. You should end up with <code>sdmc:/switch/LM-NX/LM-NX.nro</code>.</p>` +
      `<h3>4. Start the game</h3><p>Hold <strong>R</strong> while you launch any game: the Homebrew Menu opens. Choose <strong>LM-NX</strong> there. You can also make a forwarder with ${SPHAIRA}. Don't start it from the Album: it does not have enough memory.</p>` +
      `<h3>5. Change the game's language</h3><p>The USA version is in English. For Spanish, German, French or Italian, add the European disc when you make the package. The easiest way is to choose it in <strong>Game language</strong> in step 1 of this page, before you make <code>lm-nx.zip</code>. To change it later, while playing:</p><ol><li>Press <strong>L3 + R3</strong> (both sticks at once) to open the settings and go to the <strong>Game</strong> tab.</li><li>In <strong>Version to play</strong>, choose <strong>Europe</strong> and press <strong>Exit to change version</strong>.</li><li>Start LM-NX again, open the settings and choose your language in <strong>Language (European version)</strong>.</li></ol><p>The language is used for a new save. An existing save keeps its own language, which you can change in the game's Options.</p>` +
      `<p>Still stuck? See <a href="#troubleshooting">Troubleshooting</a> or <a href="#report">Report a bug</a>.</p>`,
    pageRequirements:
      `<h3>What you need</h3><ul>` +
      `<li>A Nintendo Switch that can run homebrew, with custom firmware such as <strong>Atmosphère</strong>.</li>` +
      `<li>Your own copy of <strong>Luigi's Mansion (USA)</strong> for GameCube, dumped from your disc. The ` +
      `<strong>European</strong> disc (first release) is optional: it adds the Hidden Mansion and four more languages.</li>` +
      `<li>Free space on the SD card: about <strong>1.7 GB</strong> with the USA disc, about <strong>3.2 GB</strong> with ` +
      `both discs. Your computer needs the same space for the zip.</li>` +
      `<li>A computer with an up-to-date browser to make the package.</li></ul>` +
      `<h3>How to start it</h3><p>Start <code>LM-NX.nro</code> in title takeover mode: hold <strong>R</strong> while you ` +
      `launch any game and the Homebrew Menu opens with all the memory of the console. From the Album the game does not ` +
      `have enough memory.</p>`,
    pageControls:
      `<p>The Switch controller works as a GameCube controller:</p><table class="keys"><tbody>` +
      `<tr><th>GameCube</th><th>Switch</th></tr>` +
      `<tr><td>Control stick</td><td>Left stick</td></tr><tr><td>C-stick</td><td>Right stick</td></tr>` +
      `<tr><td>A, B, X, Y</td><td>A, B, X, Y</td></tr><tr><td>Z</td><td>L or R</td></tr>` +
      `<tr><td>L, R</td><td>ZL, ZR</td></tr><tr><td>START</td><td>+</td></tr><tr><td>D-pad</td><td>D-pad</td></tr>` +
      `</tbody></table>` +
      `<p><strong>Settings menu:</strong> press both sticks at once (<strong>L3 + R3</strong>). ` +
      `There you can change the graphics, the sound, the controls and the game options. Every button can be remapped, ` +
      `and the sticks have dead zone, sensitivity and invert options. Your changes are saved when the menu closes.</p>`,
    pageFaq:
      `<h3>Why do I need my own disc?</h3><p>This page does not host or send any part of the game: it builds the ` +
      `package from the disc you own, inside your browser.</p>` +
      `<h3>Which discs work?</h3><p><strong>Luigi's Mansion (USA)</strong>, GLME01, is required. The European disc, ` +
      `GLMP01, is optional and only its first release (revision 0) works. Other releases are not supported.</p>` +
      `<h3>Can I use .rvz, .wia, .gcz or .ciso images?</h3><p>Yes: they become the full <code>.iso</code> while the ` +
      `package is made. Trimmed images such as <code>.nkit.iso</code> do not work.</p>` +
      `<h3>Is anything uploaded?</h3><p>No. Your discs are read only in your browser and the zip is saved straight to ` +
      `your computer.</p>` +
      `<h3>Where are my saves?</h3><p>In <code>sdmc:/switch/LM-NX/guardados/</code>. Copy that folder to keep a backup.</p>` +
      `<h3>How do I update?</h3><p>Make <code>lm-nx-update.zip</code> on this page and copy its <code>LM-NX</code> folder ` +
      `over the old one. Your discs and your saves stay as they are.</p>` +
      `<h3>What are the shaders?</h3><p>The game needs about 2,000 shaders, small programs for the GPU. This page ` +
      `compiles them on your computer while it makes the package and puts them in <code>cache/</code>, so the first ` +
      `start only loads them, in a few seconds.</p>` +
      `<h3>Why is my console compiling shaders?</h3><p>It happens once, with a progress bar, when the ` +
      `<code>cache/</code> folder is missing or damaged, or when you updated <code>LM-NX.nro</code> by hand instead of ` +
      `with <code>lm-nx-update.zip</code>. The shaders are saved and later starts load them in seconds.</p>` +
      `<h3>How do I change the game's language?</h3><p>The USA version is in English. For Spanish, German, French or Italian, add the European disc when you make the package. The easiest way is to choose it in <strong>Game language</strong> in step 1 of this page, before you make <code>lm-nx.zip</code>. To change it later, while playing:</p><ol><li>Press <strong>L3 + R3</strong> (both sticks at once) to open the settings and go to the <strong>Game</strong> tab.</li><li>In <strong>Version to play</strong>, choose <strong>Europe</strong> and press <strong>Exit to change version</strong>.</li><li>Start LM-NX again, open the settings and choose your language in <strong>Language (European version)</strong>.</li></ol><p>The language is used for a new save. An existing save keeps its own language, which you can change in the game's Options.</p>`,
    pageTrouble:
      `<h3>The game closes or the screen stays black when it starts</h3><p>Check that the folder is ` +
      `<code>sdmc:/switch/LM-NX/</code> with <code>LM-NX.nro</code> inside, and start it in title takeover mode (hold ` +
      `<strong>R</strong> while you launch a game), not from the Album.</p>` +
      `<h3>The game cannot find the disc</h3><p>The <code>LM-NX</code> folder must have <code>orig/GLME01.iso</code> ` +
      `inside. If it is missing, make <code>lm-nx.zip</code> again and copy the whole folder.</p>` +
      `<h3>The page does not accept my disc</h3><p>It needs a full dump or an <code>.rvz</code>, <code>.wia</code>, ` +
      `<code>.gcz</code> or <code>.ciso</code> made from one. Trimmed images and the European revision 1 do not work.</p>` +
      `<h3>The first start takes a long time</h3><p>The shaders come ready in the package. If they are missing, the ` +
      `console compiles them on the first start, which takes longer only that time.</p>` +
      `<p>Something else? See <a href="#report">Report a bug</a>.</p>`,
    pageReport:
      `<p>Found a bug? Open an issue on ${link(ISSUES, 'GitHub')}. ` +
      `Please include:</p><ul>` +
      `<li>The version of LM-NX you installed, for example 1.0.0.</li>` +
      `<li>Whether you play docked or in handheld mode.</li>` +
      `<li>What you were doing just before the problem, and whether it happens again.</li>` +
      `<li>The files in <code>sdmc:/switch/LM-NX/logs/</code>.</li>` +
      `<li>A photo or a video, if it is something you can see or hear.</li></ul>`,
    pageNews:
      `<h3>1.0.0</h3><ul><li>First release.</li>` +
      `<li>Native port of <strong>Luigi's Mansion (USA)</strong> to Nintendo Switch, based on ${RECOMP}.</li>` +
      `<li>With the European disc: the Hidden Mansion and Spanish, German, French and Italian.</li>` +
      `<li>1080p when docked.</li>` +
      `<li>Settings menu with graphics, sound, controls and game options.</li>` +
      `<li>This page makes the whole package in your browser, shaders included, and reads <code>.rvz</code>, ` +
      `<code>.wia</code>, <code>.gcz</code> and <code>.ciso</code> disc images.</li></ul>`,
    pageMedia:
      `<p>Screenshots and videos of LM-NX running on the Switch will be here soon. Meanwhile, you can find videos on ` +
      `${link(YOUTUBE, 'YouTube')}.</p>`,
    crStevens: '<strong>StevensND</strong>: LM-NX, the port to Nintendo Switch, and this page.',
    crRob: `<strong>rob646464</strong>: ${RECOMP}, the recompilation this port is based on.`,
    crDusklight: `<strong>TwilitRealm</strong> (${link(DUSKLIGHT, 'Dusklight')}): parts of JSystem and of the console's system code.`,
    crAurora: `<strong>encounter</strong> (${link(AURORA, 'Aurora')}): the GameCube compatibility layer.`,
    crSwitch: `<strong>banjo-nx</strong>, ${link(SWITCHWAKER, 'SwitchWaker')} and ${link(SWITCHWAKERHD, 'SwitchWakerHD')}: the deko3d renderer, the SDL3 layer and the shader compiler patches the port adapts.`,
    crAleks: `${link(TP_ALEKS, 'TP-Aleks')}: the Switch build setup.`,
    crDevkit: `${link(FINCS, 'fincs')} and ${link(DEVKITPRO, 'devkitPro')}: ${link(DEKO3D, 'deko3d')}, ${link(UAM, 'uam')}, ${link(LIBNX, 'libnx')} and the tools to make Switch homebrew.`,
    crDolphin: `The ${link(DOLPHIN, 'Dolphin')} team: the RVZ, WIA and GCZ formats this page reads.`,
    crLibs: '<strong>Zstandard</strong>, the <strong>LZMA SDK</strong> (Igor Pavlov), <strong>bzip2</strong> (Julian Seward), <strong>Tint</strong>, <strong>Abseil</strong> and <strong>Mesa</strong>: the decompressors and the shader compiler of this page.',
    crFont: '<strong>PFRA</strong>: Delfino, the font of the titles.',
    crNintendo: `<strong>Nintendo</strong>: Luigi's Mansion, Nintendo GameCube and Nintendo Switch are trademarks of Nintendo. LM-NX is an unofficial fan project and is not affiliated with, endorsed or sponsored by Nintendo.`,
    spzIntro:
      `<strong>Switch Port Zone</strong> is a Discord server made by ${link(CHANSEY, 'ChanseyIsTheBest')} about ports to ` +
      'Nintendo Switch. There you will find more ' +
      'ports, guides on how to make ports and people who make them, ready to help.',
    spzRules: 'Server rules',
    spzRule1: 'English only.',
    spzRule2: 'No bullying or trolling.',
    spzRule3: '<strong>No piracy.</strong>',
    spzRule4: 'Use each channel for its purpose: for example, ask for ports in #request-ports.',
    spzRule5: 'No self-promotion (your YouTube channel, your Discord server, Ko-fi, PayPal, etc.) without permission.',
    spzRule6: 'No spam. This includes spam messages like the MrBeast ones (honeypot).',
    spzRule7: 'Respect every member, as well as the admins and moderators.',
    spzRule8:
      'No alt accounts. The server detects them and using one to get around a kick or a ban can get you banned.',
    spzPiracy:
      '<strong>Piracy is not allowed, in any form.</strong> ' +
      `Don't share or ask for links to download games. Don't share videos that show links to pirated games either in ` +
      'the video or in the video description/comments. Use only the games you dumped from your own discs.',
    spzJoin: 'Join Switch Port Zone',
    backToSteps: 'Back to the installer',
    lead:
      'Builds the <strong>Nintendo Switch</strong> package of <strong>Luigi\'s Mansion</strong> from your own ' +
      '<strong>GameCube</strong> disc. <span class="line">Everything runs in <strong>this browser</strong>: nothing is uploaded.</span><br>' +
      `The port is based on ${RECOMP}, the recompilation made by <strong>rob646464</strong>.`,
    languageBar: 'Language',
    step1: '1. Choose your disc images',
    chooseIso: 'Choose disc images',
    hintBoth: 'You can choose both discs at once, or one after the other.',
    usaName: 'Luigi\'s Mansion (USA) · GLME01 · Required',
    palName: 'Luigi\'s Mansion (Europe) · GLMP01 (Rev 0) ONLY · Optional',
    usaNotChosen: 'Not chosen yet.',
    palNotChosen:
      'Not chosen. Only the first release (revision 0) works. With it, the game also gets the European ' +
      '<strong>Hidden Mansion</strong> and the other languages: <strong>Spanish, German, French and Italian</strong>.',
    ownCopy:
      '<strong>Important:</strong> you need your own copy of the game, dumped from your disc: a full disc image ' +
      '(<code>.iso</code>, 1.36 GB), <code>.rvz</code>, <code>.gcz</code> or <code>.ciso</code>. ' +
      'Compressed images become <code>.iso</code> while the package is made. ' +
      'The European disc <strong>must be the first release (revision 0)</strong>.',
    step2: '2. Create the package',
    step2First: 'If you are installing the game for the first time, use:',
    create: 'Create lm-nx.zip',
    step2Update:
      'If the game is already installed and you only want to update it (the game, its shaders and its pipeline list; ' +
      'your discs and saves stay as they are), use:',
    createUpdate: 'Create lm-nx-update.zip',
    step3: '3. Copy it to the Switch',
    step3Extract:
      'Extract the downloaded <code>.zip</code> file and copy the <code>{folder}</code> folder inside it into ' +
      '<code>sdmc:/switch/</code> on your SD card. When you update, let it replace the files that are already there.',
    step3Start:
      'Start <code>{nro}</code> from the Homebrew Menu in title takeover mode (<strong>launch</strong> a game ' +
      `<strong>while holding R</strong> to open the <strong>Homebrew Menu</strong>) or create a <strong>forwarder</strong> with ${SPHAIRA}.`,
    step3Shaders: 'The shaders are already in the package: the first start only loads them, it does not compile them.',
    sourceCode: 'Source Code',
    signature: 'a StevensND project',
    legal:
      'Luigi\'s Mansion, Nintendo GameCube and Nintendo Switch are trademarks of Nintendo. The icon is © Nintendo. LM-NX is ' +
      'an unofficial fan project and is not affiliated with, endorsed or sponsored by Nintendo. This site does not host or ' +
      'distribute disc images, game data or shaders: your disc images are read only inside your browser, are never ' +
      'uploaded, and are copied straight into the zip saved on your computer; the shaders are compiled on your computer. ' +
      'You need your own legally obtained copy of the game. Title font: Delfino by PFRA.',
    discReady: '{name} ({size}): found, ready to use.',
    palRev1: 'This is the European revision 1 (GLMP01, revision 1). The port needs the first release (revision 0).',
    otherDisc: 'This is not a supported Luigi\'s Mansion disc, or the copy has been modified.',
    notFullSize: '{name}: this is a trimmed image, not a full disc. Use a full .iso (1,459,978,240 bytes), or an .rvz, .gcz or .ciso.',
    unsupportedCompression: '{name}: this image uses a compression this page cannot read. Convert it again with Dolphin, to RVZ or to .iso.',
    badImage: '{name}: the compressed image is damaged and could not be read.',
    notGameCube: '{name}: this is not a GameCube disc image.',
    cutShort: '{name}: the disc image is incomplete (its program is cut short).',
    discId: '{text} (disc id {id})',
    discProblem: '{name}: {problem}',
    noStreaming:
      'this browser cannot save the package (it has no service workers). Use a recent Chrome, Edge or Firefox, outside ' +
      'private browsing.',
    downloading: 'Downloading {name}…',
    downloadingPipelines: 'Downloading the pipeline list…',
    downloaded: '{name} {version}: downloaded and checked.',
    downloadFailed: 'could not download {name} (HTTP {status})',
    notExpected: '{name} is not the expected file: reload the page and try again',
    compiling: 'Compiling shaders… {done} / {total}',
    compilerMismatch: 'the shader compiler ({forge}) does not match the game ({game}): reload the page',
    compiled: '{count} shaders compiled in {seconds} s.',
    buildingHM: 'Building the Hidden Mansion…',
    builtHM: 'European Hidden Mansion: built from your two discs.',
    saving: 'Saving {name} ({size}). If the browser asks where to save it, choose a folder.',
    writing: 'Writing {name}…',
    ready: '{name} is ready.',
    done: 'Done in {seconds} s. Now copy it to the Switch (step 3).',
    failed: 'The package could not be created.',
    error: 'Error: {message}',
    cancelled: 'the download was cancelled',
  },
  es: {
    menuTitle: 'Menú',
    menuOpen: 'Abrir el menú',
    menuClose: 'Cerrar el menú',
    menuLinks: 'Enlaces',
    menuCredits: 'Créditos',
    menuHelp: 'Ayuda',
    menuInstaller: 'Instalador',
    menuDisclaimer: 'Aviso',
    pageDisclaimer:
      `<ul><li><strong>LM-NX es un port hecho por la comunidad.</strong> Se basa en el trabajo de <strong>rob646464</strong> ` +
      `(${RECOMP}) adaptado a Nintendo Switch.</li>` +
      `<li><strong>Este port es gratis.</strong> No está permitido venderlo. Si pagaste por él o por que te lo ` +
      `instalaran, te han estafado.</li>` +
      `<li><strong>Las herramientas como AIO Switch Updater y similares y los sitios como NXMaster no están afiliados ` +
      `ni cuentan con el apoyo de este proyecto.</strong><br>Distribuyen el paquete con las imágenes de disco ya incluidas y ` +
      `compartir así las imágenes del juego es ilegal.<br><strong>Esta página nunca hace eso:</strong> para crear el paquete tienes que aportar tus propias imágenes de disco, sacadas de tus discos.</li>` +
      `<li><strong>Luigi's Mansion es una marca de Nintendo.</strong> Este proyecto <strong>no está afiliado a Nintendo</strong>.</li></ul>` +
      `<p><strong>Fuentes oficiales:</strong> solo esta página y el repositorio ${link(LMNX_REPO, 'lm-nx')} de ` +
      `GitHub. Cualquier otra descarga no viene de este proyecto.</p>`,
    menuAbout: 'Sobre LM-NX',
    navHowto: 'Cómo usar el instalador',
    helpButton: 'Ayuda',
    gameLanguage: `Idioma del juego`,
    gameLanguageHint: `Español, alemán, francés e italiano necesitan el disco europeo: el juego arranca entonces en la versión europea, en ese idioma. Si vuelves a crear lm-nx.zip con un idioma, se sustituyen tus ajustes.`,
    navRequirements: 'Requisitos',
    navControls: 'Controles',
    navFaq: 'Preguntas frecuentes',
    navTrouble: 'Solución de problemas',
    navReport: 'Informar de un fallo',
    navNews: 'Registro de cambios',
    navMedia: 'Capturas y vídeos',
    pageHowto:
      `<h3>Antes de empezar</h3><ul>` +
      `<li>Usa un ordenador con un navegador actualizado: el paquete se crea en tu ordenador y ocupa bastante.</li>` +
      `<li>Tu propia copia de <strong>Luigi's Mansion (USA)</strong>, extraída de tu disco, es <strong>obligatoria</strong>. El disco <strong>europeo</strong> (primera edición) es opcional: añade la Mansión oculta y más idiomas.</li>` +
      `<li>Comprueba el espacio libre que necesitas en <a href="#requirements">Requisitos</a>.</li></ul>` +
      `<h3>1. Elige tus discos</h3><p>Pulsa <strong>Elegir imágenes de disco</strong> y escoge tu <code>.iso</code>, <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> o <code>.ciso</code>. La página convierte las imágenes <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> y <code>.ciso</code> en un <code>.iso</code> completo al crear el paquete. Puedes elegir los dos discos a la vez o uno detrás de otro. Cuando un disco es válido, aparece <strong>encontrado, listo para usar</strong>. Si no lo es, el mensaje que aparece debajo te dice por qué.</p>` +
      `<h3>2. Crea el paquete</h3><p>¿Lo instalas por primera vez? Pulsa <strong>Crear lm-nx.zip</strong>. ¿Ya lo tienes instalado? Pulsa <strong>Crear lm-nx-update.zip</strong>: tus discos y tus partidas guardadas no se tocan.</p>` +
      `<p>La página descarga el paquete, lo comprueba, compila los shaders y, si añadiste el disco europeo, crea la Mansión oculta. Puede tardar unos minutos. <strong>No cierres esta pestaña</strong> hasta que ponga <strong>Terminado</strong>. Si el navegador te pregunta dónde guardar el archivo, elige una carpeta.</p>` +
      `<h3>3. Cópialo a la Switch</h3><p>Descomprime el <code>.zip</code> y copia la carpeta <code>LM-NX</code> que hay dentro en <code>sdmc:/switch/</code> de tu tarjeta SD. Debe quedar <code>sdmc:/switch/LM-NX/LM-NX.nro</code>.</p>` +
      `<h3>4. Inicia el juego</h3><p>Mantén pulsado <strong>R</strong> mientras abres cualquier juego: se abrirá el Homebrew Menu. Elige <strong>LM-NX</strong>. También puedes crear un forwarder con ${SPHAIRA}. No lo abras desde el Álbum: no tiene memoria suficiente.</p>` +
      `<h3>5. Cambia el idioma del juego</h3><p>La versión USA está en inglés. Para jugar en español, alemán, francés o italiano, añade el disco europeo al crear el paquete. Lo más fácil es elegirlo en <strong>Idioma del juego</strong>, en el paso 1 de esta página, antes de crear <code>lm-nx.zip</code>. Para cambiarlo después, durante la partida:</p><ol><li>Pulsa <strong>L3 + R3</strong> (los dos sticks a la vez) para abrir los ajustes y entra en la pestaña <strong>Game</strong>.</li><li>En <strong>Version to play</strong>, elige <strong>Europe</strong> y pulsa <strong>Exit to change version</strong>.</li><li>Vuelve a abrir LM-NX, abre los ajustes y elige tu idioma en <strong>Language (European version)</strong>.</li></ol><p>El idioma se aplica a una partida nueva. Una partida guardada mantiene su idioma, que puedes cambiar en las Opciones del juego.</p>` +
      `<p>¿Sigues con problemas? Mira <a href="#troubleshooting">Solución de problemas</a> o <a href="#report">Informar de un fallo</a>.</p>`,
    pageRequirements:
      `<h3>Qué necesitas</h3><ul>` +
      `<li>Una Nintendo Switch que pueda ejecutar homebrew, con un firmware personalizado como <strong>Atmosphère</strong>.</li>` +
      `<li>Tu propia copia de <strong>Luigi's Mansion (USA)</strong> para GameCube, sacada de tu disco. El disco ` +
      `<strong>europeo</strong> (primera edición) es opcional: añade la Mansión Oculta y cuatro idiomas más.</li>` +
      `<li>Espacio libre en la SD: unos <strong>1,7 GB</strong> con el disco USA y unos <strong>3,2 GB</strong> con los ` +
      `dos discos. Tu ordenador necesita el mismo espacio para el zip.</li>` +
      `<li>Un ordenador con un navegador actualizado para crear el paquete.</li></ul>` +
      `<h3>Cómo arrancarlo</h3><p>Abre <code>LM-NX.nro</code> en modo title takeover: mantén pulsado <strong>R</strong> ` +
      `mientras abres cualquier juego y el Homebrew Menu se abrirá con toda la memoria de la consola. Desde el Álbum el ` +
      `juego no tiene memoria suficiente.</p>`,
    pageControls:
      `<p>El mando de Switch funciona como un mando de GameCube:</p><table class="keys"><tbody>` +
      `<tr><th>GameCube</th><th>Switch</th></tr>` +
      `<tr><td>Stick de control</td><td>Stick izquierdo</td></tr><tr><td>Stick C</td><td>Stick derecho</td></tr>` +
      `<tr><td>A, B, X, Y</td><td>A, B, X, Y</td></tr><tr><td>Z</td><td>L o R</td></tr>` +
      `<tr><td>L, R</td><td>ZL, ZR</td></tr><tr><td>START</td><td>+</td></tr><tr><td>Cruceta</td><td>Cruceta</td></tr>` +
      `</tbody></table>` +
      `<p><strong>Menú de ajustes:</strong> pulsa los dos sticks a la vez (<strong>L3 + R3</strong>). ` +
      `Ahí puedes cambiar los gráficos, el sonido, los controles y las opciones del juego. Todos los botones se pueden ` +
      `reasignar, y los sticks tienen zona muerta, sensibilidad e inversión de ejes. Los cambios se guardan al cerrar ` +
      `el menú.</p>`,
    pageFaq:
      `<h3>¿Por qué necesito mi propio disco?</h3><p>Esta página no aloja ni envía ninguna parte del juego: crea el ` +
      `paquete a partir del disco que tienes, dentro de tu navegador.</p>` +
      `<h3>¿Qué discos sirven?</h3><p><strong>Luigi's Mansion (USA)</strong>, GLME01, es obligatorio. El disco europeo, ` +
      `GLMP01, es opcional y solo sirve su primera edición (revisión 0). Las demás ediciones no son compatibles.</p>` +
      `<h3>¿Puedo usar imágenes .rvz, .wia, .gcz o .ciso?</h3><p>Sí: se convierten en el <code>.iso</code> completo ` +
      `mientras se crea el paquete. Las imágenes recortadas como <code>.nkit.iso</code> no sirven.</p>` +
      `<h3>¿Se sube algo?</h3><p>No. Tus discos se leen solo en tu navegador y el zip se guarda directamente en tu ` +
      `ordenador.</p>` +
      `<h3>¿Dónde están mis partidas?</h3><p>En <code>sdmc:/switch/LM-NX/guardados/</code>. Copia esa carpeta para ` +
      `tener una copia de seguridad.</p>` +
      `<h3>¿Cómo actualizo?</h3><p>Crea <code>lm-nx-update.zip</code> en esta página y copia su carpeta ` +
      `<code>LM-NX</code> encima de la anterior. Tus discos y tus partidas se quedan como están.</p>` +
      `<h3>¿Qué son los shaders?</h3><p>El juego necesita unos 2.000 shaders, pequeños programas para la GPU. Esta ` +
      `página los compila en tu ordenador mientras crea el paquete y los guarda en <code>cache/</code>, así que el ` +
      `primer arranque solo los carga, en unos segundos.</p>` +
      `<h3>¿Por qué mi consola está compilando shaders?</h3><p>Pasa una sola vez, con una barra de progreso, cuando ` +
      `falta la carpeta <code>cache/</code> o está dañada, o cuando actualizaste <code>LM-NX.nro</code> a mano en vez ` +
      `de con <code>lm-nx-update.zip</code>. Los shaders se guardan y los siguientes arranques los cargan en segundos.</p>` +
      `<h3>¿Cómo cambio el idioma del juego?</h3><p>La versión USA está en inglés. Para jugar en español, alemán, francés o italiano, añade el disco europeo al crear el paquete. Lo más fácil es elegirlo en <strong>Idioma del juego</strong>, en el paso 1 de esta página, antes de crear <code>lm-nx.zip</code>. Para cambiarlo después, durante la partida:</p><ol><li>Pulsa <strong>L3 + R3</strong> (los dos sticks a la vez) para abrir los ajustes y entra en la pestaña <strong>Game</strong>.</li><li>En <strong>Version to play</strong>, elige <strong>Europe</strong> y pulsa <strong>Exit to change version</strong>.</li><li>Vuelve a abrir LM-NX, abre los ajustes y elige tu idioma en <strong>Language (European version)</strong>.</li></ol><p>El idioma se aplica a una partida nueva. Una partida guardada mantiene su idioma, que puedes cambiar en las Opciones del juego.</p>`,
    pageTrouble:
      `<h3>El juego se cierra o la pantalla se queda en negro al arrancar</h3><p>Comprueba que la carpeta es ` +
      `<code>sdmc:/switch/LM-NX/</code> con <code>LM-NX.nro</code> dentro, y ábrelo en modo title takeover (mantén ` +
      `pulsado <strong>R</strong> mientras abres un juego), no desde el Álbum.</p>` +
      `<h3>El juego no encuentra el disco</h3><p>La carpeta <code>LM-NX</code> tiene que tener dentro ` +
      `<code>orig/GLME01.iso</code>. Si falta, vuelve a crear <code>lm-nx.zip</code> y copia la carpeta entera.</p>` +
      `<h3>La página no acepta mi disco</h3><p>Necesita un volcado completo o un <code>.rvz</code>, <code>.wia</code>, ` +
      `<code>.gcz</code> o <code>.ciso</code> hecho a partir de uno. Las imágenes recortadas y la revisión 1 europea no ` +
      `sirven.</p>` +
      `<h3>El primer arranque tarda mucho</h3><p>Los shaders vienen listos en el paquete. Si faltan, la consola los ` +
      `compila en el primer arranque, que solo esa vez tarda más.</p>` +
      `<p>¿Otro problema? Mira <a href="#report">Informar de un fallo</a>.</p>`,
    pageReport:
      `<p>¿Has encontrado un fallo? Abre una issue en ${link(ISSUES, 'GitHub')}. ` +
      `Incluye:</p><ul>` +
      `<li>La versión de LM-NX que instalaste, por ejemplo 1.0.0.</li>` +
      `<li>Si juegas con la consola en el dock o en modo portátil.</li>` +
      `<li>Qué estabas haciendo justo antes del problema y si vuelve a pasar.</li>` +
      `<li>Los archivos de <code>sdmc:/switch/LM-NX/logs/</code>.</li>` +
      `<li>Una foto o un vídeo, si es algo que se ve o se oye.</li></ul>`,
    pageNews:
      `<h3>1.0.0</h3><ul><li>Primera versión.</li>` +
      `<li>Port nativo de <strong>Luigi's Mansion (USA)</strong> a Nintendo Switch, basado en ${RECOMP}.</li>` +
      `<li>Con el disco europeo: la Mansión Oculta y los idiomas español, alemán, francés e italiano.</li>` +
      `<li>1080p con la consola en el dock.</li>` +
      `<li>Menú de ajustes con opciones de gráficos, sonido, controles y juego.</li>` +
      `<li>Esta página crea el paquete completo en tu navegador, shaders incluidos, y lee imágenes de disco ` +
      `<code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> y <code>.ciso</code>.</li></ul>`,
    pageMedia:
      `<p>Aquí estarán pronto capturas y vídeos de LM-NX funcionando en la Switch. Mientras tanto, puedes ver vídeos en ` +
      `${link(YOUTUBE, 'YouTube')}.</p>`,
    crStevens: '<strong>StevensND</strong>: LM-NX, el port a Nintendo Switch, y esta página.',
    crRob: `<strong>rob646464</strong>: ${RECOMP}, la recompilación en la que se basa este port.`,
    crDusklight: `<strong>TwilitRealm</strong> (${link(DUSKLIGHT, 'Dusklight')}): partes de JSystem y del código de sistema de la consola.`,
    crAurora: `<strong>encounter</strong> (${link(AURORA, 'Aurora')}): la capa de compatibilidad con GameCube.`,
    crSwitch: `<strong>banjo-nx</strong>, ${link(SWITCHWAKER, 'SwitchWaker')} y ${link(SWITCHWAKERHD, 'SwitchWakerHD')}: el renderizador deko3d, la capa SDL3 y los parches del compilador de shaders que adapta el port.`,
    crAleks: `${link(TP_ALEKS, 'TP-Aleks')}: la configuración para compilar en Switch.`,
    crDevkit: `${link(FINCS, 'fincs')} y ${link(DEVKITPRO, 'devkitPro')}: ${link(DEKO3D, 'deko3d')}, ${link(UAM, 'uam')}, ${link(LIBNX, 'libnx')} y las herramientas para hacer homebrew de Switch.`,
    crDolphin: `El equipo de ${link(DOLPHIN, 'Dolphin')}: los formatos RVZ, WIA y GCZ que lee esta página.`,
    crLibs: '<strong>Zstandard</strong>, el <strong>LZMA SDK</strong> (Igor Pavlov), <strong>bzip2</strong> (Julian Seward), <strong>Tint</strong>, <strong>Abseil</strong> y <strong>Mesa</strong>: los descompresores y el compilador de shaders de esta página.',
    crFont: '<strong>PFRA</strong>: Delfino, la fuente de los títulos.',
    crNintendo: `<strong>Nintendo</strong>: Luigi's Mansion, Nintendo GameCube y Nintendo Switch son marcas de Nintendo. LM-NX es un proyecto no oficial hecho por fans y no está afiliado, respaldado ni patrocinado por Nintendo.`,
    spzIntro:
      `<strong>Switch Port Zone</strong> es un servidor de Discord creado por ${link(CHANSEY, 'ChanseyIsTheBest')} sobre ` +
      'ports a Nintendo Switch. Allí encontrarás más ' +
      'ports, guías sobre cómo hacer ports y gente que los hace, dispuesta a ayudar.',
    spzRules: 'Normas del servidor',
    spzRule1: 'Solo en inglés.',
    spzRule2: 'Nada de acoso ni de trolear.',
    spzRule3: '<strong>Nada de piratería.</strong>',
    spzRule4: 'Usa cada canal para lo que es: por ejemplo, pide ports en #request-ports.',
    spzRule5: 'Nada de autopromoción (tu canal de YouTube, tu servidor de Discord, Ko-fi, PayPal, etc.) sin permiso.',
    spzRule6: 'Nada de spam. Esto incluye mensajes de spam como los de MrBeast (honeypot).',
    spzRule7: 'Respeta a todos los miembros, así como a los administradores y moderadores.',
    spzRule8:
      'Nada de multicuentas. El servidor las detecta y usarlas para evitar un kick o un ban puede hacer que te baneen.',
    spzPiracy:
      '<strong>La piratería no está permitida de ninguna forma.</strong> ' +
      'No compartas ni pidas enlaces para descargar juegos. Tampoco compartas vídeos que muestren enlaces a juegos ' +
      'pirata, ni en el vídeo ni en su descripción o comentarios. Usa solo los juegos que hayas sacado de tus propios ' +
      'discos.',
    spzJoin: 'Unirse a Switch Port Zone',
    backToSteps: 'Volver al instalador',
    lead:
      'Crea el paquete de <strong>Nintendo Switch</strong> de <strong>Luigi\'s Mansion</strong> a partir de tu propio ' +
      'disco de <strong>GameCube</strong>. <span class="line">Todo se hace en <strong>este navegador</strong>: no se sube nada.</span><br>' +
      `El port se basa en ${RECOMP}, la recompilación hecha por <strong>rob646464</strong>.`,
    languageBar: 'Idioma',
    step1: '1. Elige tus imágenes de disco',
    chooseIso: 'Elegir imágenes de disco',
    hintBoth: 'Puedes elegir los dos discos a la vez o uno detrás de otro.',
    usaName: 'Luigi\'s Mansion (USA) · GLME01 · Obligatorio',
    palName: 'Luigi\'s Mansion (Europa) · GLMP01 (Rev 0) ÚNICAMENTE · Opcional',
    usaNotChosen: 'Todavía sin elegir.',
    palNotChosen:
      'Sin elegir. Solo sirve la primera edición (revisión 0). Con él, el juego también tiene la ' +
      '<strong>Mansión oculta</strong> europea y los demás idiomas: <strong>español, alemán, francés e italiano</strong>.',
    ownCopy:
      '<strong>Importante:</strong> necesitas tu propia copia del juego, sacada de tu disco: una imagen completa ' +
      '(<code>.iso</code>, 1,36 GB), <code>.rvz</code>, <code>.gcz</code> o <code>.ciso</code>. ' +
      'Las imágenes comprimidas pasan a <code>.iso</code> al crear el paquete. ' +
      'El disco europeo <strong>debe ser la primera edición (revisión 0)</strong>.',
    step2: '2. Crea el paquete',
    step2First: 'Si instalas el juego por primera vez, usa:',
    create: 'Crear lm-nx.zip',
    step2Update:
      'Si el juego ya está instalado y solo quieres actualizarlo (el juego, sus shaders y su lista de pipelines; tus ' +
      'discos y tus partidas se quedan como están), usa:',
    createUpdate: 'Crear lm-nx-update.zip',
    step3: '3. Cópialo a la Switch',
    step3Extract:
      'Descomprime el archivo <code>.zip</code> descargado y copia la carpeta <code>{folder}</code> que contiene en ' +
      '<code>sdmc:/switch/</code> de tu tarjeta SD. Al actualizar, deja que sustituya los archivos que ya están.',
    step3Start:
      'Abre <code>{nro}</code> desde el Homebrew Menu en modo title takeover (<strong>inicia</strong> un juego ' +
      `<strong>manteniendo pulsado R</strong> para abrir el <strong>Homebrew Menu</strong>) o crea un <strong>forwarder</strong> con ${SPHAIRA}.`,
    step3Shaders: 'Los shaders ya van en el paquete: el primer arranque solo los carga, no los compila.',
    sourceCode: 'Código fuente',
    signature: 'un proyecto de StevensND',
    legal:
      'Luigi\'s Mansion, Nintendo GameCube y Nintendo Switch son marcas comerciales de Nintendo. El icono es © Nintendo. ' +
      'LM-NX es un proyecto no oficial hecho por aficionados y no está afiliado, respaldado ni patrocinado por Nintendo. ' +
      'Este sitio no aloja ni distribuye imágenes de disco, datos del juego ni shaders: tus imágenes de disco solo se leen ' +
      'dentro de tu navegador, nunca se suben y se copian directamente al zip que se guarda en tu ordenador; los shaders ' +
      'se compilan en tu ordenador. Necesitas tu propia copia del juego obtenida legalmente. Fuente del título: Delfino, de PFRA.',
    discReady: '{name} ({size}): encontrado, listo para usar.',
    palRev1: 'Es la revisión 1 europea (GLMP01, revisión 1). El port necesita la primera edición (revisión 0).',
    otherDisc: 'No es un disco de Luigi\'s Mansion compatible, o la copia está modificada.',
    notFullSize: '{name}: es una imagen recortada, no el disco completo. Usa una .iso completa (1.459.978.240 bytes), o un .rvz, .gcz o .ciso.',
    unsupportedCompression: '{name}: esta imagen usa una compresión que esta página no sabe leer. Vuelve a convertirla con Dolphin, a RVZ o a .iso.',
    badImage: '{name}: la imagen comprimida está dañada y no se ha podido leer.',
    notGameCube: '{name}: no es una imagen de disco de GameCube.',
    cutShort: '{name}: la imagen de disco está incompleta (su programa está cortado).',
    discId: '{text} (id. del disco {id})',
    noStreaming:
      'este navegador no puede guardar el paquete (no tiene service workers). Usa un Chrome, Edge o Firefox reciente, ' +
      'fuera de la navegación privada.',
    downloading: 'Descargando {name}…',
    downloadingPipelines: 'Descargando la lista de pipelines…',
    downloaded: '{name} {version}: descargado y comprobado.',
    downloadFailed: 'no se pudo descargar {name} (HTTP {status})',
    notExpected: '{name} no es el archivo esperado: recarga la página y vuelve a intentarlo',
    compiling: 'Compilando shaders… {done} / {total}',
    compilerMismatch: 'el compilador de shaders ({forge}) no corresponde al juego ({game}): recarga la página',
    compiled: '{count} shaders compilados en {seconds} s.',
    buildingHM: 'Creando la Mansión oculta…',
    builtHM: 'Mansión oculta europea: creada a partir de tus dos discos.',
    saving: 'Guardando {name} ({size}). Si el navegador pregunta dónde guardarlo, elige una carpeta.',
    writing: 'Escribiendo {name}…',
    ready: '{name} está listo.',
    done: 'Terminado en {seconds} s. Ahora cópialo a la Switch (paso 3).',
    failed: 'No se pudo crear el paquete.',
    error: 'Error: {message}',
    cancelled: 'se canceló la descarga',
  },
  fr: {
    menuTitle: 'Menu',
    menuOpen: 'Ouvrir le menu',
    menuClose: 'Fermer le menu',
    menuLinks: 'Liens',
    menuCredits: 'Crédits',
    menuHelp: 'Aide',
    menuInstaller: 'Installateur',
    menuDisclaimer: 'Avertissement',
    pageDisclaimer:
      `<ul><li><strong>LM-NX est un portage fait par la communauté.</strong> Il repose sur le travail de <strong>rob646464</strong> ` +
      `(${RECOMP}) adapté à la Nintendo Switch.</li>` +
      `<li><strong>Ce portage est gratuit.</strong> Le vendre est interdit. Si vous avez payé pour l'avoir ou pour ` +
      `le faire installer, vous avez été arnaqué.</li>` +
      `<li><strong>Les outils comme AIO Switch Updater et similaires et les sites comme NXMaster ne sont ni affiliés à ce ` +
      `projet ni approuvés par lui.</strong><br>Ils distribuent le paquet avec les images disque déjà incluses et partager ainsi ` +
      `les images du jeu est illégal.<br><strong>Cette page ne fait jamais cela${NB}:</strong> pour créer le paquet, vous devez fournir vos propres images disque, extraites de vos disques.</li>` +
      `<li><strong>Luigi's Mansion est une marque de Nintendo.</strong> Ce projet <strong>n'est pas affilié à Nintendo</strong>.</li></ul>` +
      `<p><strong>Sources officielles${NB}:</strong> uniquement cette page et le dépôt ${link(LMNX_REPO, 'lm-nx')} ` +
      `sur GitHub. Tout autre téléchargement ne vient pas de ce projet.</p>`,
    menuAbout: 'À propos',
    navHowto: 'Comment utiliser l\'installateur',
    helpButton: 'Aide',
    gameLanguage: `Langue du jeu`,
    gameLanguageHint: `Le français, l'espagnol, l'allemand et l'italien nécessitent le disque européen${NB}: le jeu démarre alors dans la version européenne, dans cette langue. Recréer lm-nx.zip avec une langue remplace vos réglages.`,
    navRequirements: 'Configuration requise',
    navControls: 'Commandes',
    navFaq: 'FAQ',
    navTrouble: 'Dépannage',
    navReport: 'Signaler un bug',
    navNews: 'Journal des modifications',
    navMedia: 'Captures et vidéos',
    pageHowto:
      `<h3>Avant de commencer</h3><ul>` +
      `<li>Utilisez un ordinateur avec un navigateur à jour${NB}: le paquet est créé sur votre ordinateur et il est volumineux.</li>` +
      `<li>Votre propre copie de <strong>Luigi's Mansion (USA)</strong>, extraite de votre disque, est <strong>obligatoire</strong>. Le disque <strong>européen</strong> (première édition) est facultatif${NB}: il ajoute le Manoir caché et d'autres langues.</li>` +
      `<li>Vérifiez l'espace libre nécessaire dans <a href="#requirements">Configuration requise</a>.</li></ul>` +
      `<h3>1. Choisissez vos disques</h3><p>Appuyez sur <strong>Choisir les images disque</strong> et sélectionnez votre <code>.iso</code>, <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> ou <code>.ciso</code>. La page convertit les images <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> et <code>.ciso</code> en <code>.iso</code> complet en créant le paquet. Vous pouvez choisir les deux disques en même temps ou l'un après l'autre. Un disque accepté indique <strong>trouvé, prêt à l'emploi</strong>. Sinon, le message en dessous en explique la raison.</p>` +
      `<h3>2. Créez le paquet</h3><p>Première installation${NB}? Appuyez sur <strong>Créer lm-nx.zip</strong>. Déjà installé${NB}? Appuyez sur <strong>Créer lm-nx-update.zip</strong>${NB}: vos disques et vos sauvegardes restent intacts.</p>` +
      `<p>La page télécharge le paquet, le vérifie, compile les shaders et, avec le disque européen, construit le Manoir caché. Cela peut prendre quelques minutes. <strong>Gardez cet onglet ouvert</strong> jusqu'à ce qu'il affiche <strong>Terminé</strong>. Si le navigateur demande où enregistrer le fichier, choisissez un dossier.</p>` +
      `<h3>3. Copiez-le sur la Switch</h3><p>Extrayez le <code>.zip</code> et copiez le dossier <code>LM-NX</code> qu'il contient dans <code>sdmc:/switch/</code> sur votre carte SD. Vous devez obtenir <code>sdmc:/switch/LM-NX/LM-NX.nro</code>.</p>` +
      `<h3>4. Lancez le jeu</h3><p>Maintenez <strong>R</strong> en lançant n'importe quel jeu${NB}: le Homebrew Menu s'ouvre. Choisissez <strong>LM-NX</strong>. Vous pouvez aussi créer un forwarder avec ${SPHAIRA}. Ne le lancez pas depuis l'Album${NB}: il n'a pas assez de mémoire.</p>` +
      `<h3>5. Changez la langue du jeu</h3><p>La version USA est en anglais. Pour jouer en français, espagnol, allemand ou italien, ajoutez le disque européen en créant le paquet. Le plus simple est de la choisir dans <strong>Langue du jeu</strong>, à l'étape 1 de cette page, avant de créer <code>lm-nx.zip</code>. Pour la changer plus tard, en jeu${NB}:</p><ol><li>Appuyez sur <strong>L3 + R3</strong> (les deux sticks en même temps) pour ouvrir les réglages et allez dans l'onglet <strong>Game</strong>.</li><li>Dans <strong>Version to play</strong>, choisissez <strong>Europe</strong> et appuyez sur <strong>Exit to change version</strong>.</li><li>Relancez LM-NX, ouvrez les réglages et choisissez votre langue dans <strong>Language (European version)</strong>.</li></ol><p>La langue s'applique à une nouvelle sauvegarde. Une sauvegarde existante garde sa langue, que vous pouvez changer dans les Options du jeu.</p>` +
      `<p>Toujours bloqué${NB}? Consultez <a href="#troubleshooting">Dépannage</a> ou <a href="#report">Signaler un bug</a>.</p>`,
    pageRequirements:
      `<h3>Ce qu'il vous faut</h3><ul>` +
      `<li>Une Nintendo Switch capable de lancer des homebrews, avec un firmware personnalisé comme ` +
      `<strong>Atmosphère</strong>.</li>` +
      `<li>Votre propre copie de <strong>Luigi's Mansion (USA)</strong> pour GameCube, extraite de votre disque. Le disque ` +
      `<strong>européen</strong> (première édition) est facultatif${NB}: il ajoute le Manoir caché et quatre autres langues.</li>` +
      `<li>De l'espace libre sur la carte SD${NB}: environ <strong>1,7 Go</strong> avec le disque USA, environ ` +
      `<strong>3,2 Go</strong> avec les deux disques. Votre ordinateur a besoin du même espace pour le zip.</li>` +
      `<li>Un ordinateur avec un navigateur à jour pour créer le paquet.</li></ul>` +
      `<h3>Comment le lancer</h3><p>Lancez <code>LM-NX.nro</code> en mode title takeover${NB}: maintenez <strong>R</strong> ` +
      `en lançant n'importe quel jeu et le Homebrew Menu s'ouvre avec toute la mémoire de la console. Depuis l'Album, le ` +
      `jeu n'a pas assez de mémoire.</p>`,
    pageControls:
      `<p>La manette Switch fonctionne comme une manette GameCube${NB}:</p><table class="keys"><tbody>` +
      `<tr><th>GameCube</th><th>Switch</th></tr>` +
      `<tr><td>Stick de contrôle</td><td>Stick gauche</td></tr><tr><td>Stick C</td><td>Stick droit</td></tr>` +
      `<tr><td>A, B, X, Y</td><td>A, B, X, Y</td></tr><tr><td>Z</td><td>L ou R</td></tr>` +
      `<tr><td>L, R</td><td>ZL, ZR</td></tr><tr><td>START</td><td>+</td></tr><tr><td>Croix directionnelle</td>` +
      `<td>Croix directionnelle</td></tr></tbody></table>` +
      `<p><strong>Menu des réglages${NB}:</strong> appuyez sur les deux sticks en même temps ` +
      `(<strong>L3 + R3</strong>). Vous pouvez y changer les graphismes, le son, les commandes et les options du jeu. ` +
      `Tous les boutons peuvent être réattribués, et les sticks ont des réglages de zone morte, de sensibilité et ` +
      `d'inversion. Vos changements sont enregistrés à la fermeture du menu.</p>`,
    pageFaq:
      `<h3>Pourquoi ai-je besoin de mon propre disque${NB}?</h3><p>Cette page n'héberge ni n'envoie aucune partie du ` +
      `jeu${NB}: elle crée le paquet à partir du disque que vous possédez, dans votre navigateur.</p>` +
      `<h3>Quels disques fonctionnent${NB}?</h3><p><strong>Luigi's Mansion (USA)</strong>, GLME01, est obligatoire. Le ` +
      `disque européen, GLMP01, est facultatif et seule sa première édition (révision 0) fonctionne. Les autres éditions ` +
      `ne sont pas prises en charge.</p>` +
      `<h3>Puis-je utiliser des images .rvz, .wia, .gcz ou .ciso${NB}?</h3><p>Oui${NB}: elles deviennent l'<code>.iso</code> ` +
      `complet pendant la création du paquet. Les images réduites comme <code>.nkit.iso</code> ne fonctionnent pas.</p>` +
      `<h3>Quelque chose est-il envoyé${NB}?</h3><p>Non. Vos disques sont lus uniquement dans votre navigateur et le zip ` +
      `est enregistré directement sur votre ordinateur.</p>` +
      `<h3>Où sont mes sauvegardes${NB}?</h3><p>Dans <code>sdmc:/switch/LM-NX/guardados/</code>. Copiez ce dossier pour ` +
      `en garder une copie.</p>` +
      `<h3>Comment mettre à jour${NB}?</h3><p>Créez <code>lm-nx-update.zip</code> sur cette page et copiez son dossier ` +
      `<code>LM-NX</code> par-dessus l'ancien. Vos disques et vos sauvegardes restent tels quels.</p>` +
      `<h3>Que sont les shaders${NB}?</h3><p>Le jeu a besoin d'environ 2${NB}000 shaders, de petits programmes pour le ` +
      `GPU. Cette page les compile sur votre ordinateur pendant la création du paquet et les place dans ` +
      `<code>cache/</code>, si bien que le premier démarrage ne fait que les charger, en quelques secondes.</p>` +
      `<h3>Pourquoi ma console compile-t-elle des shaders${NB}?</h3><p>Cela n'arrive qu'une fois, avec une barre de ` +
      `progression, quand le dossier <code>cache/</code> manque ou est endommagé, ou quand vous avez mis à jour ` +
      `<code>LM-NX.nro</code> à la main au lieu d'utiliser <code>lm-nx-update.zip</code>. Les shaders sont enregistrés ` +
      `et les démarrages suivants les chargent en quelques secondes.</p>` +
      `<h3>Comment changer la langue du jeu${NB}?</h3><p>La version USA est en anglais. Pour jouer en français, espagnol, allemand ou italien, ajoutez le disque européen en créant le paquet. Le plus simple est de la choisir dans <strong>Langue du jeu</strong>, à l'étape 1 de cette page, avant de créer <code>lm-nx.zip</code>. Pour la changer plus tard, en jeu${NB}:</p><ol><li>Appuyez sur <strong>L3 + R3</strong> (les deux sticks en même temps) pour ouvrir les réglages et allez dans l'onglet <strong>Game</strong>.</li><li>Dans <strong>Version to play</strong>, choisissez <strong>Europe</strong> et appuyez sur <strong>Exit to change version</strong>.</li><li>Relancez LM-NX, ouvrez les réglages et choisissez votre langue dans <strong>Language (European version)</strong>.</li></ol><p>La langue s'applique à une nouvelle sauvegarde. Une sauvegarde existante garde sa langue, que vous pouvez changer dans les Options du jeu.</p>`,
    pageTrouble:
      `<h3>Le jeu se ferme ou l'écran reste noir au démarrage</h3><p>Vérifiez que le dossier est ` +
      `<code>sdmc:/switch/LM-NX/</code> avec <code>LM-NX.nro</code> dedans, et lancez-le en mode title takeover ` +
      `(maintenez <strong>R</strong> en lançant un jeu), pas depuis l'Album.</p>` +
      `<h3>Le jeu ne trouve pas le disque</h3><p>Le dossier <code>LM-NX</code> doit contenir ` +
      `<code>orig/GLME01.iso</code>. S'il manque, recréez <code>lm-nx.zip</code> et copiez le dossier entier.</p>` +
      `<h3>La page refuse mon disque</h3><p>Il faut une copie complète ou un <code>.rvz</code>, <code>.wia</code>, ` +
      `<code>.gcz</code> ou <code>.ciso</code> créé à partir de celle-ci. Les images réduites et la révision 1 ` +
      `européenne ne fonctionnent pas.</p>` +
      `<h3>Le premier démarrage est long</h3><p>Les shaders sont prêts dans le paquet. S'ils manquent, la console les ` +
      `compile au premier démarrage, qui n'est plus long que cette fois-là.</p>` +
      `<p>Autre chose${NB}? Consultez <a href="#report">Signaler un bug</a>.</p>`,
    pageReport:
      `<p>Vous avez trouvé un bug${NB}? Ouvrez une issue sur ${link(ISSUES, 'GitHub')}. ` +
      `Indiquez${NB}:</p><ul>` +
      `<li>La version de LM-NX que vous avez installée, par exemple 1.0.0.</li>` +
      `<li>Si vous jouez sur le dock ou en mode portable.</li>` +
      `<li>Ce que vous faisiez juste avant le problème, et s'il se reproduit.</li>` +
      `<li>Les fichiers de <code>sdmc:/switch/LM-NX/logs/</code>.</li>` +
      `<li>Une photo ou une vidéo, si c'est quelque chose qui se voit ou s'entend.</li></ul>`,
    pageNews:
      `<h3>1.0.0</h3><ul><li>Première version.</li>` +
      `<li>Portage natif de <strong>Luigi's Mansion (USA)</strong> sur Nintendo Switch, basé sur ${RECOMP}.</li>` +
      `<li>Avec le disque européen${NB}: le Manoir caché et l'espagnol, l'allemand, le français et l'italien.</li>` +
      `<li>1080p sur le dock.</li>` +
      `<li>Menu des réglages avec des options de graphismes, de son, de commandes et de jeu.</li>` +
      `<li>Cette page crée le paquet complet dans votre navigateur, shaders compris, et lit les images disque ` +
      `<code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> et <code>.ciso</code>.</li></ul>`,
    pageMedia:
      `<p>Des captures et des vidéos de LM-NX sur Switch seront bientôt ici. En attendant, vous trouverez des vidéos sur ` +
      `${link(YOUTUBE, 'YouTube')}.</p>`,
    crStevens: `<strong>StevensND</strong>${NB}: LM-NX, le portage sur Nintendo Switch, et cette page.`,
    crRob: `<strong>rob646464</strong>${NB}: ${RECOMP}, la recompilation sur laquelle repose ce portage.`,
    crDusklight: `<strong>TwilitRealm</strong> (${link(DUSKLIGHT, 'Dusklight')})${NB}: des parties de JSystem et du code système de la console.`,
    crAurora: `<strong>encounter</strong> (${link(AURORA, 'Aurora')})${NB}: la couche de compatibilité GameCube.`,
    crSwitch: `<strong>banjo-nx</strong>, ${link(SWITCHWAKER, 'SwitchWaker')} et ${link(SWITCHWAKERHD, 'SwitchWakerHD')}${NB}: le moteur de rendu deko3d, la couche SDL3 et les correctifs du compilateur de shaders qu'adapte le portage.`,
    crAleks: `${link(TP_ALEKS, 'TP-Aleks')}${NB}: la configuration de compilation pour Switch.`,
    crDevkit: `${link(FINCS, 'fincs')} et ${link(DEVKITPRO, 'devkitPro')}${NB}: ${link(DEKO3D, 'deko3d')}, ${link(UAM, 'uam')}, ${link(LIBNX, 'libnx')} et les outils pour créer des homebrews Switch.`,
    crDolphin: `L'équipe de ${link(DOLPHIN, 'Dolphin')}${NB}: les formats RVZ, WIA et GCZ que lit cette page.`,
    crLibs: `<strong>Zstandard</strong>, le <strong>LZMA SDK</strong> (Igor Pavlov), <strong>bzip2</strong> (Julian Seward), <strong>Tint</strong>, <strong>Abseil</strong> et <strong>Mesa</strong>${NB}: les décompresseurs et le compilateur de shaders de cette page.`,
    crFont: `<strong>PFRA</strong>${NB}: Delfino, la police des titres.`,
    crNintendo: `<strong>Nintendo</strong>${NB}: Luigi's Mansion, Nintendo GameCube et Nintendo Switch sont des marques de Nintendo. LM-NX est un projet de fans non officiel, qui n'est ni affilié, ni approuvé, ni sponsorisé par Nintendo.`,
    spzIntro:
      `<strong>Switch Port Zone</strong> est un serveur Discord créé par ${link(CHANSEY, 'ChanseyIsTheBest')}, consacré ` +
      `aux portages sur Nintendo Switch. Vous y ` +
      `trouverez d'autres portages, des guides pour créer des portages et des personnes qui en font, prêtes à vous aider.`,
    spzRules: 'Règles du serveur',
    spzRule1: 'Anglais uniquement.',
    spzRule2: 'Pas de harcèlement ni de trolling.',
    spzRule3: '<strong>Pas de piratage.</strong>',
    spzRule4: `Utilisez chaque salon pour ce à quoi il sert${NB}: par exemple, demandez des portages dans #request-ports.`,
    spzRule5: `Pas d'autopromotion (votre chaîne YouTube, votre serveur Discord, Ko-fi, PayPal, etc.) sans autorisation.`,
    spzRule6: 'Pas de spam. Cela inclut les messages de spam comme ceux de MrBeast (honeypot).',
    spzRule7: 'Respectez tous les membres, ainsi que les administrateurs et les modérateurs.',
    spzRule8:
      'Pas de comptes multiples. Le serveur les détecte et en utiliser un pour contourner un kick ou un ban peut vous ' +
      'valoir un ban.',
    spzPiracy:
      `<strong>Le piratage n'est autorisé sous aucune forme.</strong> ` +
      'Ne partagez pas et ne demandez pas de liens pour télécharger des jeux. Ne partagez pas non plus de vidéos ' +
      'montrant des liens vers des jeux piratés, ni dans la vidéo ni dans sa description ou ses commentaires. Utilisez ' +
      'uniquement les jeux extraits de vos propres disques.',
    spzJoin: 'Rejoindre Switch Port Zone',
    backToSteps: `Retour à l'installateur`,
    lead:
      'Crée le paquet <strong>Nintendo Switch</strong> de <strong>Luigi\'s Mansion</strong> à partir de votre propre ' +
      `disque <strong>GameCube</strong>. <span class="line">Tout se passe dans <strong>ce navigateur</strong>${NB}: rien n'est envoyé.</span><br>` +
      `Le portage est basé sur ${RECOMP}, la recompilation réalisée par <strong>rob646464</strong>.`,
    languageBar: 'Langue',
    step1: '1. Choisissez vos images disque',
    chooseIso: 'Choisir les images disque',
    hintBoth: 'Vous pouvez choisir les deux disques en même temps, ou l\'un après l\'autre.',
    usaName: 'Luigi\'s Mansion (USA) · GLME01 · Obligatoire',
    palName: 'Luigi\'s Mansion (Europe) · GLMP01 (Rev 0) UNIQUEMENT · Facultatif',
    usaNotChosen: 'Pas encore choisi.',
    palNotChosen:
      'Non choisi. Seule la première édition (révision 0) fonctionne. Avec lui, le jeu obtient aussi le ' +
      `<strong>Manoir caché</strong> européen et les autres langues${NB}: <strong>espagnol, allemand, français et italien</strong>.`,
    ownCopy:
      `<strong>Important${NB}:</strong> il vous faut votre copie du jeu, extraite de votre disque${NB}: ` +
      'une image complète (<code>.iso</code>, 1,36 Go), <code>.rvz</code>, <code>.gcz</code> ou ' +
      `<code>.ciso</code>. Les images compressées passent en <code>.iso</code> en créant le paquet. ` +
      'Le disque européen <strong>doit être la première édition (révision 0)</strong>.',
    step2: '2. Créez le paquet',
    step2First: `Si vous installez le jeu pour la première fois, utilisez${NB}:`,
    create: 'Créer lm-nx.zip',
    step2Update:
      'Si le jeu est déjà installé et que vous voulez seulement le mettre à jour (le jeu, ses shaders et sa liste de ' +
      `pipelines${NB}; vos disques et vos sauvegardes restent tels quels), utilisez${NB}:`,
    createUpdate: 'Créer lm-nx-update.zip',
    step3: '3. Copiez-le sur la Switch',
    step3Extract:
      'Extrayez le fichier <code>.zip</code> téléchargé et copiez le dossier <code>{folder}</code> qu\'il contient dans ' +
      '<code>sdmc:/switch/</code> sur votre carte SD. Lors d\'une mise à jour, laissez-le remplacer les fichiers déjà présents.',
    step3Start:
      'Lancez <code>{nro}</code> depuis le Homebrew Menu en mode title takeover (<strong>lancez</strong> un jeu ' +
      `<strong>en maintenant R</strong> pour ouvrir le <strong>Homebrew Menu</strong>) ou créez un <strong>forwarder</strong> avec ${SPHAIRA}.`,
    step3Shaders:
      `Les shaders sont déjà dans le paquet${NB}: le premier démarrage se contente de les charger, il ne les compile pas.`,
    sourceCode: 'Code source',
    signature: 'un projet de StevensND',
    legal:
      'Luigi\'s Mansion, Nintendo GameCube et Nintendo Switch sont des marques de Nintendo. L\'icône est © Nintendo. LM-NX ' +
      'est un projet de fans non officiel, qui n\'est ni affilié à Nintendo, ni approuvé ni sponsorisé par Nintendo. Ce ' +
      `site n'héberge ni ne distribue d'images disque, de données du jeu ni de shaders${NB}: vos images disque sont lues ` +
      'uniquement dans votre navigateur, ne sont jamais envoyées et sont copiées directement dans le zip enregistré sur ' +
      `votre ordinateur${NB}; les shaders sont compilés sur votre ordinateur. Vous devez posséder votre propre copie du jeu ` +
      `obtenue légalement. Police du titre${NB}: Delfino, de PFRA.`,
    discReady: `{name} ({size})${NB}: trouvé, prêt à l'emploi.`,
    palRev1: 'C\'est la révision 1 européenne (GLMP01, révision 1). Le portage a besoin de la première édition (révision 0).',
    otherDisc: 'Ce n\'est pas un disque de Luigi\'s Mansion pris en charge, ou la copie a été modifiée.',
    notFullSize: `{name}${NB}: c'est une image réduite, pas le disque complet. Utilisez un .iso complet (1${NB}459${NB}978${NB}240 octets), ou un .rvz, .gcz ou .ciso.`,
    unsupportedCompression: `{name}${NB}: cette image utilise une compression que cette page ne sait pas lire. Reconvertissez-la avec Dolphin, en RVZ ou en .iso.`,
    badImage: `{name}${NB}: l'image compressée est endommagée et n'a pas pu être lue.`,
    notGameCube: `{name}${NB}: ce n'est pas une image disque GameCube.`,
    cutShort: `{name}${NB}: l'image disque est incomplète (son programme est tronqué).`,
    discId: '{text} (identifiant du disque {id})',
    noStreaming:
      `ce navigateur ne peut pas enregistrer le paquet (il n'a pas de service workers). Utilisez un Chrome, Edge ou ` +
      'Firefox récent, hors navigation privée.',
    downloading: 'Téléchargement de {name}…',
    downloadingPipelines: 'Téléchargement de la liste des pipelines…',
    downloaded: `{name} {version}${NB}: téléchargé et vérifié.`,
    downloadFailed: 'impossible de télécharger {name} (HTTP {status})',
    notExpected: `{name} n'est pas le fichier attendu${NB}: rechargez la page et réessayez`,
    compiling: 'Compilation des shaders… {done} / {total}',
    compilerMismatch: `le compilateur de shaders ({forge}) ne correspond pas au jeu ({game})${NB}: rechargez la page`,
    compiled: '{count} shaders compilés en {seconds} s.',
    buildingHM: 'Création du Manoir caché…',
    builtHM: `Manoir caché européen${NB}: créé à partir de vos deux disques.`,
    saving: 'Enregistrement de {name} ({size}). Si le navigateur demande où l\'enregistrer, choisissez un dossier.',
    writing: 'Écriture de {name}…',
    ready: '{name} est prêt.',
    done: 'Terminé en {seconds} s. Copiez-le maintenant sur la Switch (étape 3).',
    failed: 'Le paquet n\'a pas pu être créé.',
    error: `Erreur${NB}: {message}`,
    cancelled: 'le téléchargement a été annulé',
  },
  de: {
    menuTitle: 'Menü',
    menuOpen: 'Menü öffnen',
    menuClose: 'Menü schließen',
    menuLinks: 'Links',
    menuCredits: 'Credits',
    menuHelp: 'Hilfe',
    menuInstaller: 'Installer',
    menuDisclaimer: 'Hinweis',
    pageDisclaimer:
      `<ul><li><strong>LM-NX ist eine Portierung aus der Community.</strong> Sie basiert auf der Arbeit von <strong>rob646464</strong> ` +
      `(${RECOMP}), angepasst an die Nintendo Switch.</li>` +
      `<li><strong>Diese Portierung ist kostenlos.</strong> Sie zu verkaufen ist nicht erlaubt. Wenn du dafür oder ` +
      `für die Installation bezahlt hast, wurdest du betrogen.</li>` +
      `<li><strong>Werkzeuge wie AIO Switch Updater und ähnliche sowie Seiten wie NXMaster stehen in keiner Verbindung zu ` +
      `diesem Projekt und werden von ihm nicht unterstützt.</strong><br>Sie verbreiten das Paket mit bereits enthaltenen ` +
      `Disc-Images und die Images des Spiels so weiterzugeben ist illegal.<br><strong>Diese Seite macht das nie:</strong> Für das Paket musst du deine eigenen Disc-Images bereitstellen, ausgelesen von deinen Discs.</li>` +
      `<li><strong>Luigi's Mansion ist eine Marke von Nintendo.</strong> Dieses Projekt <strong>steht in keiner Verbindung zu Nintendo</strong>.</li></ul>` +
      `<p><strong>Offizielle Quellen:</strong> nur diese Seite und das Repository ${link(LMNX_REPO, 'lm-nx')} auf ` +
      `GitHub. Jeder andere Download stammt nicht von diesem Projekt.</p>`,
    menuAbout: 'Über LM-NX',
    navHowto: 'So benutzt du den Installer',
    helpButton: 'Hilfe',
    gameLanguage: `Sprache des Spiels`,
    gameLanguageHint: `Deutsch, Spanisch, Französisch und Italienisch brauchen die europäische Disc: Das Spiel startet dann in der europäischen Version in dieser Sprache. Wenn du lm-nx.zip mit einer Sprache neu erstellst, werden deine Einstellungen ersetzt.`,
    navRequirements: 'Voraussetzungen',
    navControls: 'Steuerung',
    navFaq: 'FAQ',
    navTrouble: 'Fehlerbehebung',
    navReport: 'Fehler melden',
    navNews: 'Änderungsprotokoll',
    navMedia: 'Bilder und Videos',
    pageHowto:
      `<h3>Bevor du anfängst</h3><ul>` +
      `<li>Nutze einen Computer mit einem aktuellen Browser: Das Paket wird auf deinem Computer erstellt und ist groß.</li>` +
      `<li>Deine eigene Kopie von <strong>Luigi's Mansion (USA)</strong>, von deiner Disc ausgelesen, ist <strong>erforderlich</strong>. Die <strong>europäische</strong> Disc (erste Auflage) ist optional: Sie bringt die Versteckte Villa und weitere Sprachen.</li>` +
      `<li>Den nötigen freien Speicher findest du unter <a href="#requirements">Voraussetzungen</a>.</li></ul>` +
      `<h3>1. Wähle deine Discs</h3><p>Drücke <strong>Disc-Images wählen</strong> und wähle deine <code>.iso</code>, <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> oder <code>.ciso</code>. Die Seite wandelt <code>.rvz</code>-, <code>.wia</code>-, <code>.gcz</code>- und <code>.ciso</code>-Images beim Erstellen des Pakets in eine vollständige <code>.iso</code> um. Du kannst beide Discs zusammen oder nacheinander wählen. Eine passende Disc zeigt <strong>gefunden, bereit</strong>. Wenn nicht, steht darunter der Grund.</p>` +
      `<h3>2. Erstelle das Paket</h3><p>Erste Installation? Drücke <strong>lm-nx.zip erstellen</strong>. Schon installiert? Drücke <strong>lm-nx-update.zip erstellen</strong>: Deine Discs und Spielstände bleiben, wie sie sind.</p>` +
      `<p>Die Seite lädt das Paket herunter, prüft es, kompiliert die Shader und baut mit der europäischen Disc die Versteckte Villa. Das kann ein paar Minuten dauern. <strong>Lass diesen Tab offen</strong>, bis <strong>Fertig</strong> erscheint. Fragt der Browser, wo die Datei gespeichert werden soll, wähle einen Ordner.</p>` +
      `<h3>3. Auf die Switch kopieren</h3><p>Entpacke die <code>.zip</code> und kopiere den Ordner <code>LM-NX</code> darin nach <code>sdmc:/switch/</code> auf deiner SD-Karte. Am Ende gibt es <code>sdmc:/switch/LM-NX/LM-NX.nro</code>.</p>` +
      `<h3>4. Spiel starten</h3><p>Halte <strong>R</strong> gedrückt, während du ein beliebiges Spiel startest: Das Homebrew Menu öffnet sich. Wähle dort <strong>LM-NX</strong>. Du kannst auch einen Forwarder mit ${SPHAIRA} erstellen. Starte es nicht über das Album: Dort reicht der Speicher nicht.</p>` +
      `<h3>5. Sprache des Spiels ändern</h3><p>Die USA-Version ist auf Englisch. Für Deutsch, Spanisch, Französisch oder Italienisch füge beim Erstellen des Pakets die europäische Disc hinzu. Am einfachsten wählst du sie unter <strong>Sprache des Spiels</strong> in Schritt 1 dieser Seite, bevor du <code>lm-nx.zip</code> erstellst. Um sie später zu ändern, im Spiel:</p><ol><li>Drücke <strong>L3 + R3</strong> (beide Sticks gleichzeitig), um die Einstellungen zu öffnen, und gehe zum Reiter <strong>Game</strong>.</li><li>Wähle bei <strong>Version to play</strong> die Option <strong>Europe</strong> und drücke <strong>Exit to change version</strong>.</li><li>Starte LM-NX erneut, öffne die Einstellungen und wähle deine Sprache bei <strong>Language (European version)</strong>.</li></ol><p>Die Sprache gilt für einen neuen Spielstand. Ein vorhandener Spielstand behält seine Sprache, die du in den Optionen des Spiels ändern kannst.</p>` +
      `<p>Klappt es noch nicht? Sieh unter <a href="#troubleshooting">Fehlerbehebung</a> nach oder <a href="#report">melde einen Fehler</a>.</p>`,
    pageRequirements:
      `<h3>Was du brauchst</h3><ul>` +
      `<li>Eine Nintendo Switch, die Homebrew ausführen kann, mit einer Custom Firmware wie <strong>Atmosphère</strong>.</li>` +
      `<li>Deine eigene Kopie von <strong>Luigi's Mansion (USA)</strong> für GameCube, von deiner Disc ausgelesen. Die ` +
      `<strong>europäische</strong> Disc (erste Auflage) ist optional: Sie bringt die Versteckte Villa und vier weitere ` +
      `Sprachen.</li>` +
      `<li>Freier Platz auf der SD-Karte: etwa <strong>1,7 GB</strong> mit der US-Disc, etwa <strong>3,2 GB</strong> mit ` +
      `beiden Discs. Dein Computer braucht denselben Platz für das Zip.</li>` +
      `<li>Einen Computer mit einem aktuellen Browser, um das Paket zu erstellen.</li></ul>` +
      `<h3>So startest du es</h3><p>Starte <code>LM-NX.nro</code> im Title-Takeover-Modus: Halte <strong>R</strong> ` +
      `gedrückt, während du ein beliebiges Spiel startest, dann öffnet sich das Homebrew-Menü mit dem ganzen Speicher ` +
      `der Konsole. Aus dem Album heraus hat das Spiel nicht genug Speicher.</p>`,
    pageControls:
      `<p>Der Switch-Controller funktioniert wie ein GameCube-Controller:</p><table class="keys"><tbody>` +
      `<tr><th>GameCube</th><th>Switch</th></tr>` +
      `<tr><td>Control-Stick</td><td>Linker Stick</td></tr><tr><td>C-Stick</td><td>Rechter Stick</td></tr>` +
      `<tr><td>A, B, X, Y</td><td>A, B, X, Y</td></tr><tr><td>Z</td><td>L oder R</td></tr>` +
      `<tr><td>L, R</td><td>ZL, ZR</td></tr><tr><td>START</td><td>+</td></tr><tr><td>Steuerkreuz</td>` +
      `<td>Steuerkreuz</td></tr></tbody></table>` +
      `<p><strong>Einstellungsmenü:</strong> Drücke beide Sticks gleichzeitig ` +
      `(<strong>L3 + R3</strong>). Dort kannst du Grafik, Ton, Steuerung und Spieloptionen ändern. Jede Taste lässt sich ` +
      `neu belegen, und für die Sticks gibt es Totzone, Empfindlichkeit und Invertierung. Deine Änderungen werden beim ` +
      `Schließen des Menüs gespeichert.</p>`,
    pageFaq:
      `<h3>Warum brauche ich meine eigene Disc?</h3><p>Diese Seite stellt keinen Teil des Spiels bereit und verschickt ` +
      `auch keinen: Sie erstellt das Paket aus der Disc, die du besitzt, in deinem Browser.</p>` +
      `<h3>Welche Discs funktionieren?</h3><p><strong>Luigi's Mansion (USA)</strong>, GLME01, ist nötig. Die europäische ` +
      `Disc, GLMP01, ist optional, und nur ihre erste Auflage (Revision 0) funktioniert. Andere Auflagen werden nicht ` +
      `unterstützt.</p>` +
      `<h3>Kann ich .rvz-, .wia-, .gcz- oder .ciso-Images nutzen?</h3><p>Ja: Sie werden beim Erstellen des Pakets zur ` +
      `vollständigen <code>.iso</code>. Gekürzte Images wie <code>.nkit.iso</code> funktionieren nicht.</p>` +
      `<h3>Wird etwas hochgeladen?</h3><p>Nein. Deine Discs werden nur in deinem Browser gelesen, und das Zip wird direkt ` +
      `auf deinem Computer gespeichert.</p>` +
      `<h3>Wo sind meine Spielstände?</h3><p>In <code>sdmc:/switch/LM-NX/guardados/</code>. Kopiere diesen Ordner als ` +
      `Sicherung.</p>` +
      `<h3>Wie aktualisiere ich?</h3><p>Erstelle auf dieser Seite <code>lm-nx-update.zip</code> und kopiere seinen Ordner ` +
      `<code>LM-NX</code> über den alten. Deine Discs und Spielstände bleiben, wie sie sind.</p>` +
      `<h3>Was sind die Shader?</h3><p>Das Spiel braucht etwa 2.000 Shader, kleine Programme für die GPU. Diese Seite ` +
      `kompiliert sie beim Erstellen des Pakets auf deinem Computer und legt sie in <code>cache/</code>, sodass der ` +
      `erste Start sie nur in wenigen Sekunden lädt.</p>` +
      `<h3>Warum kompiliert meine Konsole Shader?</h3><p>Das passiert nur einmal, mit einem Fortschrittsbalken, wenn ` +
      `der Ordner <code>cache/</code> fehlt oder beschädigt ist oder wenn du <code>LM-NX.nro</code> von Hand statt mit ` +
      `<code>lm-nx-update.zip</code> aktualisiert hast. Die Shader werden gespeichert und spätere Starts laden sie in ` +
      `Sekunden.</p>` +
      `<h3>Wie ändere ich die Sprache des Spiels?</h3><p>Die USA-Version ist auf Englisch. Für Deutsch, Spanisch, Französisch oder Italienisch füge beim Erstellen des Pakets die europäische Disc hinzu. Am einfachsten wählst du sie unter <strong>Sprache des Spiels</strong> in Schritt 1 dieser Seite, bevor du <code>lm-nx.zip</code> erstellst. Um sie später zu ändern, im Spiel:</p><ol><li>Drücke <strong>L3 + R3</strong> (beide Sticks gleichzeitig), um die Einstellungen zu öffnen, und gehe zum Reiter <strong>Game</strong>.</li><li>Wähle bei <strong>Version to play</strong> die Option <strong>Europe</strong> und drücke <strong>Exit to change version</strong>.</li><li>Starte LM-NX erneut, öffne die Einstellungen und wähle deine Sprache bei <strong>Language (European version)</strong>.</li></ol><p>Die Sprache gilt für einen neuen Spielstand. Ein vorhandener Spielstand behält seine Sprache, die du in den Optionen des Spiels ändern kannst.</p>`,
    pageTrouble:
      `<h3>Das Spiel schließt sich oder der Bildschirm bleibt beim Start schwarz</h3><p>Prüfe, dass der Ordner ` +
      `<code>sdmc:/switch/LM-NX/</code> ist und <code>LM-NX.nro</code> enthält, und starte es im Title-Takeover-Modus ` +
      `(halte <strong>R</strong> gedrückt, während du ein Spiel startest), nicht aus dem Album.</p>` +
      `<h3>Das Spiel findet die Disc nicht</h3><p>Im Ordner <code>LM-NX</code> muss <code>orig/GLME01.iso</code> liegen. ` +
      `Fehlt sie, erstelle <code>lm-nx.zip</code> neu und kopiere den ganzen Ordner.</p>` +
      `<h3>Die Seite nimmt meine Disc nicht an</h3><p>Sie braucht ein vollständiges Abbild oder eine daraus erstellte ` +
      `<code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> oder <code>.ciso</code>. Gekürzte Images und die ` +
      `europäische Revision 1 funktionieren nicht.</p>` +
      `<h3>Der erste Start dauert lange</h3><p>Die Shader liegen fertig im Paket. Fehlen sie, kompiliert die Konsole sie ` +
      `beim ersten Start, der nur dieses eine Mal länger dauert.</p>` +
      `<p>Etwas anderes? Siehe <a href="#report">Fehler melden</a>.</p>`,
    pageReport:
      `<p>Einen Fehler gefunden? Eröffne ein Issue auf ${link(ISSUES, 'GitHub')}. ` +
      `Gib bitte an:</p><ul>` +
      `<li>Die installierte Version von LM-NX, zum Beispiel 1.0.0.</li>` +
      `<li>Ob du im Dock oder im Handheld-Modus spielst.</li>` +
      `<li>Was du kurz vor dem Problem gemacht hast und ob es wieder passiert.</li>` +
      `<li>Die Dateien aus <code>sdmc:/switch/LM-NX/logs/</code>.</li>` +
      `<li>Ein Foto oder Video, wenn man das Problem sehen oder hören kann.</li></ul>`,
    pageNews:
      `<h3>1.0.0</h3><ul><li>Erste Version.</li>` +
      `<li>Native Portierung von <strong>Luigi's Mansion (USA)</strong> für die Nintendo Switch, basierend auf ${RECOMP}.</li>` +
      `<li>Mit der europäischen Disc: die Versteckte Villa sowie Spanisch, Deutsch, Französisch und Italienisch.</li>` +
      `<li>1080p im Dock.</li>` +
      `<li>Einstellungsmenü mit Optionen für Grafik, Ton, Steuerung und Spiel.</li>` +
      `<li>Diese Seite erstellt das ganze Paket in deinem Browser, Shader inklusive, und liest die Disc-Images ` +
      `<code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> und <code>.ciso</code>.</li></ul>`,
    pageMedia:
      `<p>Hier gibt es bald Bilder und Videos von LM-NX auf der Switch. Bis dahin findest du Videos auf ` +
      `${link(YOUTUBE, 'YouTube')}.</p>`,
    crStevens: '<strong>StevensND</strong>: LM-NX, die Portierung für Nintendo Switch, und diese Seite.',
    crRob: `<strong>rob646464</strong>: ${RECOMP}, die Rekompilierung, auf der diese Portierung basiert.`,
    crDusklight: `<strong>TwilitRealm</strong> (${link(DUSKLIGHT, 'Dusklight')}): Teile von JSystem und vom Systemcode der Konsole.`,
    crAurora: `<strong>encounter</strong> (${link(AURORA, 'Aurora')}): die GameCube-Kompatibilitätsschicht.`,
    crSwitch: `<strong>banjo-nx</strong>, ${link(SWITCHWAKER, 'SwitchWaker')} und ${link(SWITCHWAKERHD, 'SwitchWakerHD')}: der deko3d-Renderer, die SDL3-Schicht und die Patches des Shader-Compilers, die die Portierung übernimmt.`,
    crAleks: `${link(TP_ALEKS, 'TP-Aleks')}: die Build-Einrichtung für die Switch.`,
    crDevkit: `${link(FINCS, 'fincs')} und ${link(DEVKITPRO, 'devkitPro')}: ${link(DEKO3D, 'deko3d')}, ${link(UAM, 'uam')}, ${link(LIBNX, 'libnx')} und die Werkzeuge für Switch-Homebrew.`,
    crDolphin: `Das ${link(DOLPHIN, 'Dolphin')}-Team: die Formate RVZ, WIA und GCZ, die diese Seite liest.`,
    crLibs: '<strong>Zstandard</strong>, das <strong>LZMA SDK</strong> (Igor Pavlov), <strong>bzip2</strong> (Julian Seward), <strong>Tint</strong>, <strong>Abseil</strong> und <strong>Mesa</strong>: die Dekompressoren und der Shader-Compiler dieser Seite.',
    crFont: '<strong>PFRA</strong>: Delfino, die Schrift der Titel.',
    crNintendo: `<strong>Nintendo</strong>: Luigi's Mansion, Nintendo GameCube und Nintendo Switch sind Marken von Nintendo. LM-NX ist ein inoffizielles Fanprojekt und steht in keiner Verbindung zu Nintendo, wird von Nintendo weder unterstützt noch gesponsert.`,
    spzIntro:
      `<strong>Switch Port Zone</strong> ist ein Discord-Server von ${link(CHANSEY, 'ChanseyIsTheBest')} rund um Ports ` +
      'für die Nintendo Switch. Dort findest du ' +
      'weitere Ports, Anleitungen zum Erstellen von Ports und Leute, die Ports machen und gerne helfen.',
    spzRules: 'Serverregeln',
    spzRule1: 'Nur Englisch.',
    spzRule2: 'Kein Mobbing und kein Trolling.',
    spzRule3: '<strong>Keine Piraterie.</strong>',
    spzRule4: 'Nutze jeden Kanal für seinen Zweck: Wünsche dir zum Beispiel Ports in #request-ports.',
    spzRule5: 'Keine Eigenwerbung (dein YouTube-Kanal, dein Discord-Server, Ko-fi, PayPal usw.) ohne Erlaubnis.',
    spzRule6: 'Kein Spam. Dazu gehören auch Spam-Nachrichten wie die von MrBeast (Honeypot).',
    spzRule7: 'Respektiere alle Mitglieder sowie die Admins und Moderatoren.',
    spzRule8:
      'Keine Zweitaccounts. Der Server erkennt sie und wer einen nutzt, um einen Kick oder Bann zu umgehen, kann ' +
      'gebannt werden.',
    spzPiracy:
      '<strong>Piraterie ist in keiner Form erlaubt.</strong> ' +
      'Teile keine Links zum Herunterladen von Spielen und frage nicht danach. Teile auch keine Videos, die Links zu ' +
      'raubkopierten Spielen zeigen, weder im Video noch in seiner Beschreibung oder den Kommentaren. Nutze nur Spiele, ' +
      'die du von deinen eigenen Discs ausgelesen hast.',
    spzJoin: 'Switch Port Zone beitreten',
    backToSteps: 'Zurück zum Installer',
    lead:
      'Erstellt das <strong>Nintendo Switch</strong>-Paket von <strong>Luigi\'s Mansion</strong> aus deiner eigenen ' +
      '<strong>GameCube</strong>-Disc. <span class="line">Alles läuft in <strong>diesem Browser</strong>: Es wird nichts hochgeladen.</span><br>' +
      `Die Portierung basiert auf ${RECOMP}, der Rekompilierung von <strong>rob646464</strong>.`,
    languageBar: 'Sprache',
    step1: '1. Wähle deine Disc-Images',
    chooseIso: 'Disc-Images wählen',
    hintBoth: 'Du kannst beide Discs auf einmal oder nacheinander wählen.',
    usaName: 'Luigi\'s Mansion (USA) · GLME01 · Erforderlich',
    palName: 'Luigi\'s Mansion (Europa) · GLMP01 (Rev 0) NUR · Optional',
    usaNotChosen: 'Noch nicht gewählt.',
    palNotChosen:
      'Nicht gewählt. Nur die erste Auflage (Revision 0) funktioniert. Damit bekommt das Spiel auch die europäische ' +
      '<strong>Versteckte Villa</strong> und die anderen Sprachen: <strong>Spanisch, Deutsch, Französisch und Italienisch</strong>.',
    ownCopy:
      '<strong>Wichtig:</strong> Du brauchst deine eigene Kopie des Spiels von deiner Disc: ein vollständiges Image ' +
      '(<code>.iso</code>, 1,36 GB), <code>.rvz</code>, <code>.gcz</code> oder <code>.ciso</code>. ' +
      'Komprimierte Images werden beim Erstellen zur <code>.iso</code>. ' +
      'Die europäische Disc <strong>muss die erste Auflage (Revision 0) sein</strong>.',
    step2: '2. Erstelle das Paket',
    step2First: 'Wenn du das Spiel zum ersten Mal installierst, nimm:',
    create: 'lm-nx.zip erstellen',
    step2Update:
      'Wenn das Spiel schon installiert ist und du es nur aktualisieren willst (das Spiel, seine Shader und seine ' +
      'Pipeline-Liste; deine Discs und Spielstände bleiben, wie sie sind), nimm:',
    createUpdate: 'lm-nx-update.zip erstellen',
    step3: '3. Kopiere es auf die Switch',
    step3Extract:
      'Entpacke die heruntergeladene <code>.zip</code>-Datei und kopiere den Ordner <code>{folder}</code> darin nach ' +
      '<code>sdmc:/switch/</code> auf deiner SD-Karte. Bei einem Update lass ihn die vorhandenen Dateien ersetzen.',
    step3Start:
      'Starte <code>{nro}</code> aus dem Homebrew Menu im Title-Takeover-Modus (<strong>starte</strong> ein Spiel ' +
      `<strong>mit gedrückter R-Taste</strong>, um das <strong>Homebrew Menu</strong> zu öffnen) oder erstelle einen <strong>Forwarder</strong> mit ${SPHAIRA}.`,
    step3Shaders: 'Die Shader sind schon im Paket: Der erste Start lädt sie nur, er kompiliert sie nicht.',
    sourceCode: 'Quellcode',
    signature: 'ein Projekt von StevensND',
    legal:
      'Luigi\'s Mansion, Nintendo GameCube und Nintendo Switch sind Marken von Nintendo. Das Symbol ist © Nintendo. LM-NX ' +
      'ist ein inoffizielles Fanprojekt und steht in keiner Verbindung zu Nintendo, wird von Nintendo weder unterstützt ' +
      'noch gesponsert. Diese Seite hostet oder verbreitet keine Disc-Images, Spieldaten oder Shader: Deine Disc-Images ' +
      'werden nur in deinem Browser gelesen, nie hochgeladen und direkt in die ZIP-Datei auf deinem Computer kopiert; die ' +
      'Shader werden auf deinem Computer kompiliert. Du brauchst deine eigene, legal erworbene Kopie des Spiels. Schriftart des Titels: Delfino von PFRA.',
    discReady: '{name} ({size}): gefunden, bereit.',
    palRev1: 'Das ist die europäische Revision 1 (GLMP01, Revision 1). Der Port braucht die erste Auflage (Revision 0).',
    otherDisc: 'Das ist keine unterstützte Luigi\'s-Mansion-Disc, oder die Kopie wurde verändert.',
    notFullSize: '{name}: Das ist ein gekürztes Image, nicht die ganze Disc. Nimm eine vollständige .iso (1.459.978.240 Bytes) oder eine .rvz, .gcz oder .ciso.',
    unsupportedCompression: '{name}: Dieses Image nutzt eine Kompression, die diese Seite nicht lesen kann. Wandle es mit Dolphin erneut um, in RVZ oder in eine .iso.',
    badImage: '{name}: Das komprimierte Image ist beschädigt und konnte nicht gelesen werden.',
    notGameCube: '{name}: Das ist kein GameCube-Disc-Image.',
    cutShort: '{name}: Das Disc-Image ist unvollständig (sein Programm ist abgeschnitten).',
    discId: '{text} (Disc-ID {id})',
    noStreaming:
      'Dieser Browser kann das Paket nicht speichern (er hat keine Service Worker). Nimm ein aktuelles Chrome, Edge oder ' +
      'Firefox, nicht im privaten Modus.',
    downloading: '{name} wird heruntergeladen…',
    downloadingPipelines: 'Die Pipeline-Liste wird heruntergeladen…',
    downloaded: '{name} {version}: heruntergeladen und geprüft.',
    downloadFailed: '{name} konnte nicht heruntergeladen werden (HTTP {status})',
    notExpected: '{name} ist nicht die erwartete Datei: Lade die Seite neu und versuche es noch einmal',
    compiling: 'Shader werden kompiliert… {done} / {total}',
    compilerMismatch: 'Der Shader-Compiler ({forge}) passt nicht zum Spiel ({game}): Lade die Seite neu',
    compiled: '{count} Shader in {seconds} s kompiliert.',
    buildingHM: 'Die Versteckte Villa wird erstellt…',
    builtHM: 'Europäische Versteckte Villa: aus deinen beiden Discs erstellt.',
    saving: '{name} wird gespeichert ({size}). Wenn der Browser fragt, wo es gespeichert werden soll, wähle einen Ordner.',
    writing: '{name} wird geschrieben…',
    ready: '{name} ist fertig.',
    done: 'Fertig in {seconds} s. Kopiere es jetzt auf die Switch (Schritt 3).',
    failed: 'Das Paket konnte nicht erstellt werden.',
    error: 'Fehler: {message}',
    cancelled: 'der Download wurde abgebrochen',
  },
  it: {
    menuTitle: 'Menu',
    menuOpen: 'Apri il menu',
    menuClose: 'Chiudi il menu',
    menuLinks: 'Link',
    menuCredits: 'Crediti',
    menuHelp: 'Aiuto',
    menuInstaller: 'Installer',
    menuDisclaimer: 'Avvertenza',
    pageDisclaimer:
      `<ul><li><strong>LM-NX è un port creato dalla comunità.</strong> Si basa sul lavoro di <strong>rob646464</strong> ` +
      `(${RECOMP}) adattato a Nintendo Switch.</li>` +
      `<li><strong>Questo port è gratuito.</strong> Non è permesso venderlo. Se hai pagato per averlo o per fartelo ` +
      `installare, sei stato truffato.</li>` +
      `<li><strong>Gli strumenti come AIO Switch Updater e simili e i siti come NXMaster non sono affiliati a questo progetto ` +
      `né approvati da esso.</strong><br>Distribuiscono il pacchetto con le immagini disco già incluse e condividere così le ` +
      `immagini del gioco è illegale.<br><strong>Questa pagina non lo fa mai:</strong> per creare il pacchetto devi ` +
      `fornire tu stesso le tue immagini disco, estratte dai tuoi dischi.</li>` +
      `<li><strong>Luigi's Mansion è un marchio di Nintendo.</strong> Questo progetto <strong>non è affiliato a Nintendo</strong>.</li></ul>` +
      `<p><strong>Fonti ufficiali:</strong> solo questa pagina e il repository ${link(LMNX_REPO, 'lm-nx')} su ` +
      `GitHub. Qualsiasi altro download non proviene da questo progetto.</p>`,
    menuAbout: 'Info su LM-NX',
    navHowto: 'Come usare l\'installer',
    helpButton: 'Aiuto',
    gameLanguage: `Lingua del gioco`,
    gameLanguageHint: `Italiano, spagnolo, tedesco e francese richiedono il disco europeo: il gioco si avvia allora nella versione europea, in quella lingua. Se ricrei lm-nx.zip con una lingua, le tue impostazioni vengono sostituite.`,
    navRequirements: 'Requisiti',
    navControls: 'Comandi',
    navFaq: 'Domande frequenti',
    navTrouble: 'Risoluzione dei problemi',
    navReport: 'Segnala un bug',
    navNews: 'Registro delle modifiche',
    navMedia: 'Immagini e video',
    pageHowto:
      `<h3>Prima di iniziare</h3><ul>` +
      `<li>Usa un computer con un browser aggiornato: il pacchetto viene creato sul tuo computer ed è grande.</li>` +
      `<li>La tua copia di <strong>Luigi's Mansion (USA)</strong>, estratta dal tuo disco, è <strong>obbligatoria</strong>. Il disco <strong>europeo</strong> (prima edizione) è facoltativo: aggiunge la Villa Nascosta e altre lingue.</li>` +
      `<li>Controlla lo spazio libero necessario in <a href="#requirements">Requisiti</a>.</li></ul>` +
      `<h3>1. Scegli i dischi</h3><p>Premi <strong>Scegli le immagini disco</strong> e seleziona il tuo <code>.iso</code>, <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> o <code>.ciso</code>. La pagina converte le immagini <code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> e <code>.ciso</code> in un <code>.iso</code> completo mentre crea il pacchetto. Puoi scegliere entrambi i dischi insieme o uno dopo l'altro. Un disco accettato mostra <strong>trovato, pronto all'uso</strong>. Altrimenti il messaggio sotto spiega il motivo.</p>` +
      `<h3>2. Crea il pacchetto</h3><p>Prima installazione? Premi <strong>Crea lm-nx.zip</strong>. Già installato? Premi <strong>Crea lm-nx-update.zip</strong>: i tuoi dischi e i salvataggi restano come sono.</p>` +
      `<p>La pagina scarica il pacchetto, lo controlla, compila gli shader e, con il disco europeo, costruisce la Villa Nascosta. Può richiedere qualche minuto. <strong>Tieni aperta questa scheda</strong> finché non compare <strong>Fatto</strong>. Se il browser chiede dove salvare il file, scegli una cartella.</p>` +
      `<h3>3. Copialo sulla Switch</h3><p>Estrai il <code>.zip</code> e copia la cartella <code>LM-NX</code> al suo interno in <code>sdmc:/switch/</code> sulla scheda SD. Alla fine deve esserci <code>sdmc:/switch/LM-NX/LM-NX.nro</code>.</p>` +
      `<h3>4. Avvia il gioco</h3><p>Tieni premuto <strong>R</strong> mentre avvii un gioco qualsiasi: si apre l'Homebrew Menu. Scegli <strong>LM-NX</strong>. Puoi anche creare un forwarder con ${SPHAIRA}. Non avviarlo dall'Album: non ha memoria sufficiente.</p>` +
      `<h3>5. Cambia la lingua del gioco</h3><p>La versione USA è in inglese. Per giocare in italiano, spagnolo, tedesco o francese, aggiungi il disco europeo quando crei il pacchetto. Il modo più semplice è sceglierla in <strong>Lingua del gioco</strong>, al passo 1 di questa pagina, prima di creare <code>lm-nx.zip</code>. Per cambiarla dopo, durante il gioco:</p><ol><li>Premi <strong>L3 + R3</strong> (entrambe le levette insieme) per aprire le impostazioni e vai alla scheda <strong>Game</strong>.</li><li>In <strong>Version to play</strong>, scegli <strong>Europe</strong> e premi <strong>Exit to change version</strong>.</li><li>Riavvia LM-NX, apri le impostazioni e scegli la lingua in <strong>Language (European version)</strong>.</li></ol><p>La lingua vale per un nuovo salvataggio. Un salvataggio esistente mantiene la sua lingua, che puoi cambiare nelle Opzioni del gioco.</p>` +
      `<p>Ancora problemi? Vedi <a href="#troubleshooting">Risoluzione dei problemi</a> o <a href="#report">Segnala un bug</a>.</p>`,
    pageRequirements:
      `<h3>Cosa ti serve</h3><ul>` +
      `<li>Una Nintendo Switch in grado di avviare homebrew, con un firmware personalizzato come <strong>Atmosphère</strong>.</li>` +
      `<li>La tua copia di <strong>Luigi's Mansion (USA)</strong> per GameCube, estratta dal tuo disco. Il disco ` +
      `<strong>europeo</strong> (prima edizione) è facoltativo: aggiunge la Villa Nascosta e altre quattro lingue.</li>` +
      `<li>Spazio libero sulla scheda SD: circa <strong>1,7 GB</strong> con il disco USA, circa <strong>3,2 GB</strong> ` +
      `con entrambi i dischi. Il tuo computer ha bisogno dello stesso spazio per lo zip.</li>` +
      `<li>Un computer con un browser aggiornato per creare il pacchetto.</li></ul>` +
      `<h3>Come avviarlo</h3><p>Avvia <code>LM-NX.nro</code> in modalità title takeover: tieni premuto <strong>R</strong> ` +
      `mentre avvii un gioco qualsiasi e l'Homebrew Menu si apre con tutta la memoria della console. Dall'Album il gioco ` +
      `non ha abbastanza memoria.</p>`,
    pageControls:
      `<p>Il controller Switch funziona come un controller GameCube:</p><table class="keys"><tbody>` +
      `<tr><th>GameCube</th><th>Switch</th></tr>` +
      `<tr><td>Levetta di controllo</td><td>Levetta sinistra</td></tr><tr><td>Levetta C</td><td>Levetta destra</td></tr>` +
      `<tr><td>A, B, X, Y</td><td>A, B, X, Y</td></tr><tr><td>Z</td><td>L o R</td></tr>` +
      `<tr><td>L, R</td><td>ZL, ZR</td></tr><tr><td>START</td><td>+</td></tr><tr><td>Croce direzionale</td>` +
      `<td>Croce direzionale</td></tr></tbody></table>` +
      `<p><strong>Menu delle impostazioni:</strong> premi entrambe le levette insieme ` +
      `(<strong>L3 + R3</strong>). Lì puoi cambiare grafica, audio, comandi e opzioni di gioco. Ogni tasto si può ` +
      `riassegnare, e le levette hanno zona morta, sensibilità e inversione degli assi. Le modifiche vengono salvate ` +
      `quando chiudi il menu.</p>`,
    pageFaq:
      `<h3>Perché mi serve il mio disco?</h3><p>Questa pagina non ospita né invia nessuna parte del gioco: crea il ` +
      `pacchetto dal disco che possiedi, nel tuo browser.</p>` +
      `<h3>Quali dischi funzionano?</h3><p><strong>Luigi's Mansion (USA)</strong>, GLME01, è obbligatorio. Il disco ` +
      `europeo, GLMP01, è facoltativo e funziona solo la sua prima edizione (revisione 0). Le altre edizioni non sono ` +
      `supportate.</p>` +
      `<h3>Posso usare immagini .rvz, .wia, .gcz o .ciso?</h3><p>Sì: diventano la <code>.iso</code> completa mentre si ` +
      `crea il pacchetto. Le immagini ridotte come <code>.nkit.iso</code> non funzionano.</p>` +
      `<h3>Viene caricato qualcosa?</h3><p>No. I tuoi dischi vengono letti solo nel tuo browser e lo zip viene salvato ` +
      `direttamente sul tuo computer.</p>` +
      `<h3>Dove sono i miei salvataggi?</h3><p>In <code>sdmc:/switch/LM-NX/guardados/</code>. Copia quella cartella per ` +
      `averne un backup.</p>` +
      `<h3>Come aggiorno?</h3><p>Crea <code>lm-nx-update.zip</code> in questa pagina e copia la sua cartella ` +
      `<code>LM-NX</code> sopra quella vecchia. I tuoi dischi e i tuoi salvataggi restano come sono.</p>` +
      `<h3>Cosa sono gli shader?</h3><p>Il gioco ha bisogno di circa 2.000 shader, piccoli programmi per la GPU. ` +
      `Questa pagina li compila sul tuo computer mentre crea il pacchetto e li mette in <code>cache/</code>, così il ` +
      `primo avvio li carica soltanto, in pochi secondi.</p>` +
      `<h3>Perché la mia console sta compilando gli shader?</h3><p>Succede una sola volta, con una barra di ` +
      `avanzamento, quando la cartella <code>cache/</code> manca o è danneggiata, oppure quando hai aggiornato ` +
      `<code>LM-NX.nro</code> a mano invece che con <code>lm-nx-update.zip</code>. Gli shader vengono salvati e gli ` +
      `avvii successivi li caricano in pochi secondi.</p>` +
      `<h3>Come cambio la lingua del gioco?</h3><p>La versione USA è in inglese. Per giocare in italiano, spagnolo, tedesco o francese, aggiungi il disco europeo quando crei il pacchetto. Il modo più semplice è sceglierla in <strong>Lingua del gioco</strong>, al passo 1 di questa pagina, prima di creare <code>lm-nx.zip</code>. Per cambiarla dopo, durante il gioco:</p><ol><li>Premi <strong>L3 + R3</strong> (entrambe le levette insieme) per aprire le impostazioni e vai alla scheda <strong>Game</strong>.</li><li>In <strong>Version to play</strong>, scegli <strong>Europe</strong> e premi <strong>Exit to change version</strong>.</li><li>Riavvia LM-NX, apri le impostazioni e scegli la lingua in <strong>Language (European version)</strong>.</li></ol><p>La lingua vale per un nuovo salvataggio. Un salvataggio esistente mantiene la sua lingua, che puoi cambiare nelle Opzioni del gioco.</p>`,
    pageTrouble:
      `<h3>Il gioco si chiude o lo schermo resta nero all'avvio</h3><p>Controlla che la cartella sia ` +
      `<code>sdmc:/switch/LM-NX/</code> con dentro <code>LM-NX.nro</code>, e avvialo in modalità title takeover (tieni ` +
      `premuto <strong>R</strong> mentre avvii un gioco), non dall'Album.</p>` +
      `<h3>Il gioco non trova il disco</h3><p>Nella cartella <code>LM-NX</code> deve esserci <code>orig/GLME01.iso</code>. ` +
      `Se manca, crea di nuovo <code>lm-nx.zip</code> e copia tutta la cartella.</p>` +
      `<h3>La pagina non accetta il mio disco</h3><p>Serve un dump completo oppure un <code>.rvz</code>, <code>.wia</code>, ` +
      `<code>.gcz</code> o <code>.ciso</code> creato da esso. Le immagini ridotte e la revisione 1 europea non ` +
      `funzionano.</p>` +
      `<h3>Il primo avvio è lento</h3><p>Gli shader sono già pronti nel pacchetto. Se mancano, la console li compila al ` +
      `primo avvio, che solo quella volta richiede più tempo.</p>` +
      `<p>Altro? Vedi <a href="#report">Segnala un bug</a>.</p>`,
    pageReport:
      `<p>Hai trovato un bug? Apri una issue su ${link(ISSUES, 'GitHub')}. ` +
      `Indica:</p><ul>` +
      `<li>La versione di LM-NX che hai installato, per esempio 1.0.0.</li>` +
      `<li>Se giochi con la console nel dock o in modalità portatile.</li>` +
      `<li>Cosa stavi facendo subito prima del problema e se si ripete.</li>` +
      `<li>I file in <code>sdmc:/switch/LM-NX/logs/</code>.</li>` +
      `<li>Una foto o un video, se è qualcosa che si vede o si sente.</li></ul>`,
    pageNews:
      `<h3>1.0.0</h3><ul><li>Prima versione.</li>` +
      `<li>Port nativo di <strong>Luigi's Mansion (USA)</strong> per Nintendo Switch, basato su ${RECOMP}.</li>` +
      `<li>Con il disco europeo: la Villa Nascosta e spagnolo, tedesco, francese e italiano.</li>` +
      `<li>1080p con la console nel dock.</li>` +
      `<li>Menu delle impostazioni con opzioni di grafica, audio, comandi e gioco.</li>` +
      `<li>Questa pagina crea il pacchetto completo nel tuo browser, shader compresi, e legge le immagini disco ` +
      `<code>.rvz</code>, <code>.wia</code>, <code>.gcz</code> e <code>.ciso</code>.</li></ul>`,
    pageMedia:
      `<p>Presto qui ci saranno immagini e video di LM-NX su Switch. Nel frattempo puoi trovare dei video su ` +
      `${link(YOUTUBE, 'YouTube')}.</p>`,
    crStevens: '<strong>StevensND</strong>: LM-NX, il port per Nintendo Switch, e questa pagina.',
    crRob: `<strong>rob646464</strong>: ${RECOMP}, la ricompilazione su cui si basa questo port.`,
    crDusklight: `<strong>TwilitRealm</strong> (${link(DUSKLIGHT, 'Dusklight')}): parti di JSystem e del codice di sistema della console.`,
    crAurora: `<strong>encounter</strong> (${link(AURORA, 'Aurora')}): lo strato di compatibilità con GameCube.`,
    crSwitch: `<strong>banjo-nx</strong>, ${link(SWITCHWAKER, 'SwitchWaker')} e ${link(SWITCHWAKERHD, 'SwitchWakerHD')}: il renderer deko3d, lo strato SDL3 e le patch del compilatore di shader che il port adatta.`,
    crAleks: `${link(TP_ALEKS, 'TP-Aleks')}: la configurazione per compilare su Switch.`,
    crDevkit: `${link(FINCS, 'fincs')} e ${link(DEVKITPRO, 'devkitPro')}: ${link(DEKO3D, 'deko3d')}, ${link(UAM, 'uam')}, ${link(LIBNX, 'libnx')} e gli strumenti per creare homebrew per Switch.`,
    crDolphin: `Il team di ${link(DOLPHIN, 'Dolphin')}: i formati RVZ, WIA e GCZ che legge questa pagina.`,
    crLibs: '<strong>Zstandard</strong>, il <strong>LZMA SDK</strong> (Igor Pavlov), <strong>bzip2</strong> (Julian Seward), <strong>Tint</strong>, <strong>Abseil</strong> e <strong>Mesa</strong>: i decompressori e il compilatore di shader di questa pagina.',
    crFont: '<strong>PFRA</strong>: Delfino, il carattere dei titoli.',
    crNintendo: `<strong>Nintendo</strong>: Luigi's Mansion, Nintendo GameCube e Nintendo Switch sono marchi di Nintendo. LM-NX è un progetto amatoriale non ufficiale e non è affiliato, approvato né sponsorizzato da Nintendo.`,
    spzIntro:
      `<strong>Switch Port Zone</strong> è un server Discord creato da ${link(CHANSEY, 'ChanseyIsTheBest')} e dedicato ` +
      'ai port per Nintendo Switch. Lì troverai altri ' +
      'port, guide su come creare port e persone che li realizzano, pronte ad aiutarti.',
    spzRules: 'Regole del server',
    spzRule1: 'Solo in inglese.',
    spzRule2: 'Niente bullismo né trolling.',
    spzRule3: '<strong>Niente pirateria.</strong>',
    spzRule4: 'Usa ogni canale per il suo scopo: per esempio, chiedi i port in #request-ports.',
    spzRule5: 'Niente autopromozione (il tuo canale YouTube, il tuo server Discord, Ko-fi, PayPal, ecc.) senza permesso.',
    spzRule6: 'Niente spam. Questo include i messaggi di spam come quelli di MrBeast (honeypot).',
    spzRule7: 'Rispetta tutti i membri, così come gli amministratori e i moderatori.',
    spzRule8:
      'Niente account multipli. Il server li rileva e usarli per aggirare un kick o un ban può farti bannare.',
    spzPiracy:
      '<strong>La pirateria non è permessa in nessuna forma.</strong> ' +
      'Non condividere e non chiedere link per scaricare giochi. Non condividere nemmeno video che mostrino link a ' +
      'giochi pirata, né nel video né nella sua descrizione o nei commenti. Usa solo i giochi estratti dai tuoi dischi.',
    spzJoin: 'Unisciti a Switch Port Zone',
    backToSteps: `Torna all'installer`,
    lead:
      'Crea il pacchetto per <strong>Nintendo Switch</strong> di <strong>Luigi\'s Mansion</strong> dal tuo disco ' +
      '<strong>GameCube</strong>. <span class="line">Tutto avviene in <strong>questo browser</strong>: non viene caricato nulla.</span><br>' +
      `Il port si basa su ${RECOMP}, la ricompilazione realizzata da <strong>rob646464</strong>.`,
    languageBar: 'Lingua',
    step1: '1. Scegli le tue immagini disco',
    chooseIso: 'Scegli le immagini disco',
    hintBoth: 'Puoi scegliere entrambi i dischi insieme oppure uno dopo l\'altro.',
    usaName: 'Luigi\'s Mansion (USA) · GLME01 · Obbligatorio',
    palName: 'Luigi\'s Mansion (Europa) · GLMP01 (Rev 0) SOLTANTO · Facoltativo',
    usaNotChosen: 'Non ancora scelto.',
    palNotChosen:
      'Non scelto. Funziona solo la prima edizione (revisione 0). Con questo disco il gioco ha anche il ' +
      '<strong>Palazzo Nascosto</strong> europeo e le altre lingue: <strong>spagnolo, tedesco, francese e italiano</strong>.',
    ownCopy:
      '<strong>Importante:</strong> ti serve la tua copia del gioco, estratta dal disco: un\'immagine completa ' +
      '(<code>.iso</code>, 1,36 GB), <code>.rvz</code>, <code>.gcz</code> o <code>.ciso</code>. ' +
      'Le immagini compresse diventano <code>.iso</code> creando il pacchetto. ' +
      'Il disco europeo <strong>deve essere la prima edizione (revisione 0)</strong>.',
    step2: '2. Crea il pacchetto',
    step2First: 'Se installi il gioco per la prima volta, usa:',
    create: 'Crea lm-nx.zip',
    step2Update:
      'Se il gioco è già installato e vuoi solo aggiornarlo (il gioco, i suoi shader e la sua lista di pipeline; i tuoi ' +
      'dischi e i tuoi salvataggi restano come sono), usa:',
    createUpdate: 'Crea lm-nx-update.zip',
    step3: '3. Copialo sulla Switch',
    step3Extract:
      'Estrai il file <code>.zip</code> scaricato e copia la cartella <code>{folder}</code> che contiene in ' +
      '<code>sdmc:/switch/</code> sulla tua scheda SD. Quando aggiorni, lascia che sostituisca i file già presenti.',
    step3Start:
      'Avvia <code>{nro}</code> dal Homebrew Menu in modalità title takeover (<strong>avvia</strong> un gioco ' +
      `<strong>tenendo premuto R</strong> per aprire l'<strong>Homebrew Menu</strong>) oppure crea un <strong>forwarder</strong> con ${SPHAIRA}.`,
    step3Shaders: 'Gli shader sono già nel pacchetto: il primo avvio li carica soltanto, non li compila.',
    sourceCode: 'Codice sorgente',
    signature: 'un progetto di StevensND',
    legal:
      'Luigi\'s Mansion, Nintendo GameCube e Nintendo Switch sono marchi di Nintendo. L\'icona è © Nintendo. LM-NX è un ' +
      'progetto amatoriale non ufficiale e non è affiliato, approvato né sponsorizzato da Nintendo. Questo sito non ospita ' +
      'né distribuisce immagini disco, dati di gioco o shader: le tue immagini disco vengono lette solo nel tuo browser, ' +
      'non vengono mai caricate e vengono copiate direttamente nello zip salvato sul tuo computer; gli shader vengono ' +
      'compilati sul tuo computer. Ti serve una copia del gioco ottenuta legalmente. Carattere del titolo: Delfino di PFRA.',
    discReady: '{name} ({size}): trovato, pronto all\'uso.',
    palRev1: 'Questa è la revisione 1 europea (GLMP01, revisione 1). Il port ha bisogno della prima edizione (revisione 0).',
    otherDisc: 'Questo non è un disco di Luigi\'s Mansion supportato, oppure la copia è stata modificata.',
    notFullSize: '{name}: questa è un\'immagine ridotta, non il disco completo. Usa una .iso completa (1.459.978.240 byte), oppure un .rvz, .gcz o .ciso.',
    unsupportedCompression: '{name}: questa immagine usa una compressione che questa pagina non sa leggere. Riconvertila con Dolphin, in RVZ o in .iso.',
    badImage: '{name}: l\'immagine compressa è danneggiata e non è stato possibile leggerla.',
    notGameCube: '{name}: non è un\'immagine disco GameCube.',
    cutShort: '{name}: l\'immagine disco è incompleta (il suo programma è troncato).',
    discId: '{text} (ID disco {id})',
    noStreaming:
      'questo browser non può salvare il pacchetto (non ha i service worker). Usa un Chrome, Edge o Firefox recente, ' +
      'non in navigazione privata.',
    downloading: 'Download di {name}…',
    downloadingPipelines: 'Download della lista di pipeline…',
    downloaded: '{name} {version}: scaricato e verificato.',
    downloadFailed: 'impossibile scaricare {name} (HTTP {status})',
    notExpected: '{name} non è il file previsto: ricarica la pagina e riprova',
    compiling: 'Compilazione degli shader… {done} / {total}',
    compilerMismatch: 'il compilatore di shader ({forge}) non corrisponde al gioco ({game}): ricarica la pagina',
    compiled: '{count} shader compilati in {seconds} s.',
    buildingHM: 'Creazione del Palazzo Nascosto…',
    builtHM: 'Palazzo Nascosto europeo: creato dai tuoi due dischi.',
    saving: 'Salvataggio di {name} ({size}). Se il browser chiede dove salvarlo, scegli una cartella.',
    writing: 'Scrittura di {name}…',
    ready: '{name} è pronto.',
    done: 'Fatto in {seconds} s. Ora copialo sulla Switch (passo 3).',
    failed: 'Non è stato possibile creare il pacchetto.',
    error: 'Errore: {message}',
    cancelled: 'il download è stato annullato',
  },
};

let current = 'en';

export function getLanguage() {
  return current;
}

export function setLanguage(code) {
  current = TEXTS[code] ? code : 'en';
  return current;
}

// English unless the visitor chose another language with the flags (saved in their browser).
export function preferredLanguage(saved) {
  return saved && TEXTS[saved] ? saved : 'en';
}

export function t(key, params = {}) {
  const text = TEXTS[current][key] ?? TEXTS.en[key] ?? key;
  return text.replace(/\{(\w+)\}/g, (_, name) => (params[name] ?? `{${name}}`));
}

// Sizes in units of 1024 bytes, with the decimal separator of the current language.
export function formatSize(bytes) {
  const units = current === 'fr' ? ['Ko', 'Mo', 'Go'] : ['KB', 'MB', 'GB'];
  const [value, unit, decimals] = bytes >= 1024 ** 3 ? [bytes / 1024 ** 3, 2, 2]
    : bytes >= 1024 ** 2 ? [bytes / 1024 ** 2, 1, 1] : [Math.max(1, bytes / 1024), 0, 0];
  const number = new Intl.NumberFormat(current, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  return `${number.format(value)}${current === 'fr' ? NB : ' '}${units[unit]}`;
}
