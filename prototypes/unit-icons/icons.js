// Each icon is drawn on a 100x100 grid. "currentColor" = ink, "var(--paper)" = knockout.
const INK = 'currentColor', CUT = 'var(--paper)';
const ICONS = {
  extraction: `
    <rect x="31" y="3" width="36" height="13" rx="4" fill="${INK}"/>
    <path d="M32 19 H66 V30 L52 90 H46 L32 30 Z" fill="${INK}"/>
    <g stroke="${CUT}" stroke-width="3.2">
      <line x1="28" y1="42" x2="70" y2="31"/><line x1="28" y1="56" x2="70" y2="45"/>
      <line x1="28" y1="70" x2="70" y2="59"/>
    </g>
    <g fill="${INK}" stroke="${INK}" stroke-width="2" stroke-linejoin="round">
      <polygon points="1,85 7,73 18,71 27,82 24,95 4,95"/>
      <polygon points="9,62 15,57 21,61 18,68 11,67"/>
      <polygon points="22,71 28,69 31,74 27,78 21,76"/>
      <polygon points="27,88 33,84 38,89 35,95 28,95"/>
      <circle cx="27" cy="62" r="1.8"/>
      <polygon points="56,85 62,73 76,72 83,84 78,95 59,95"/>
      <polygon points="72,58 80,53 87,59 83,67 74,66"/>
      <polygon points="86,84 92,78 98,84 96,92 88,92"/>
      <polygon points="61,66 66,64 68,69 63,71"/>
    </g>`,

  farming: `
    <path d="M8 93 Q50 70 92 93 Z" fill="${INK}"/>
    <g stroke="${INK}" stroke-width="5.5" fill="none" stroke-linecap="round">
      <path d="M48 86 C48 72 46 62 41 53"/><path d="M47 74 C49 62 52 54 57 47"/>
    </g>
    <path d="M43 54 C28 60 6 50 3 20 C27 16 45 30 43 54 Z" fill="${INK}"/>
    <path d="M55 48 C54 20 74 8 99 8 C97 36 78 50 55 48 Z" fill="${INK}"/>
    <g stroke="${CUT}" stroke-width="3" fill="none" stroke-linecap="round">
      <path d="M40 50 Q28 38 13 28"/><path d="M58 44 Q72 30 89 16"/>
    </g>`,

  manufacturing: `
    <rect x="1" y="78" width="98" height="19" rx="9.5" fill="${INK}"/>
    <g fill="${CUT}">${[12, 30, 48, 66, 84].map(x => `<circle cx="${x+2}" cy="87.5" r="4.2"/>`).join('')}</g>
    <rect x="8" y="66" width="32" height="8" rx="1.5" fill="${INK}"/>
    <rect x="53" y="62" width="11" height="12" fill="${INK}"/>
    <rect x="71" y="62" width="12" height="12" fill="${INK}"/>
    <g stroke="${INK}" stroke-width="10" stroke-linecap="round" fill="none">
      <path d="M26 64 L22 33 L58 11 L78 35"/>
    </g>
    ${[[22, 33], [58, 11]].map(([x, y]) => `
      <circle cx="${x}" cy="${y}" r="9.5" fill="${INK}"/>
      <circle cx="${x}" cy="${y}" r="5" fill="${CUT}"/>
      <circle cx="${x}" cy="${y}" r="2.4" fill="${INK}"/>`).join('')}
    <line x1="78" y1="35" x2="78" y2="41" stroke="${INK}" stroke-width="4"/>
    <path d="M68 56 V48 Q68 40 78 40 Q88 40 88 48 V56" stroke="${INK}" stroke-width="4" fill="none" stroke-linecap="round"/>
    <path d="M68 56 L73 52 M88 56 L83 52" stroke="${INK}" stroke-width="3.5" stroke-linecap="round"/>`,

  transport: `
    <rect x="5" y="22" width="56" height="40" rx="4" stroke="${INK}" stroke-width="5" fill="none"/>
    <path d="M14 30 V56 M14 30 H54" stroke="${INK}" stroke-width="3" fill="none"/>
    <path d="M1 34 V56" stroke="${INK}" stroke-width="4" stroke-linecap="round"/>
    <path d="M64 36 H81 L94 51 V68 H64 Z" stroke="${INK}" stroke-width="5" fill="none" stroke-linejoin="round"/>
    <path d="M69 42 H78 L87 53 H69 Z" fill="${INK}"/>
    <rect x="2" y="61" width="94" height="9" rx="3" fill="${INK}"/>
    ${[17, 40, 80].map(x => `
      <circle cx="${x}" cy="79" r="13" fill="${CUT}"/>
      <circle cx="${x}" cy="79" r="10.5" fill="${INK}"/>
      <circle cx="${x}" cy="79" r="4" fill="${CUT}"/>`).join('')}`,

  communication: `
    <g stroke="${INK}" stroke-width="5" fill="none" stroke-linecap="round">
      <path d="M40 20 A14 14 0 0 1 60 20"/><path d="M31 11 A27 27 0 0 1 69 11"/>
    </g>
    <rect x="42" y="28" width="16" height="66" rx="8" fill="${INK}"/>
    <g stroke="${CUT}" stroke-width="3"><line x1="40" y1="44" x2="60" y2="44"/><line x1="40" y1="74" x2="60" y2="74"/></g>
    ${[[19, 52, 30], [81, 52, -30]].map(([x, y, a]) => `
      <g transform="translate(${x} ${y}) rotate(${a})" fill="${INK}">
        <rect x="-18" y="-13" width="17" height="12"/><rect x="1" y="-13" width="17" height="12"/>
        <rect x="-18" y="1" width="17" height="12"/><rect x="1" y="1" width="17" height="12"/>
      </g>`).join('')}`,

  research: `
    <path d="M42 9 V38 L12 84 Q7 94 18 94 H82 Q93 94 88 84 L58 38 V9" stroke="${INK}" stroke-width="6" fill="none" stroke-linejoin="round"/>
    <line x1="35" y1="7" x2="65" y2="7" stroke="${INK}" stroke-width="6" stroke-linecap="round"/>
    <path d="M36 58 H64 L80 82 Q83 88 76 88 H24 Q17 88 20 82 Z" fill="${INK}"/>
    <g fill="${CUT}"><circle cx="45" cy="70" r="4"/><circle cx="58" cy="64" r="3"/><circle cx="58" cy="79" r="4.5"/></g>`,

  energy: `
    <rect x="40" y="10" width="20" height="5" rx="2" fill="${INK}"/>
    <rect x="46" y="13" width="8" height="12" fill="${INK}"/>
    <path d="M13 64 A37 37 0 0 1 87 64" stroke="${INK}" stroke-width="5" fill="none"/>
    <path d="M6 89 L8 64 H18 L19 89 Z M94 89 L92 64 H82 L81 89 Z" fill="${INK}"/>
    <path d="M21 89 V62 A29 29 0 0 1 79 62 V89 Z" fill="${INK}"/>
    <path d="M56 38 L41 63 H50 L45 82 L61 55 H52 Z" fill="${CUT}"/>
    <rect x="1" y="88" width="98" height="6" rx="1" fill="${INK}"/>`,

  construction: `
    <g stroke="${INK}" stroke-width="2.6" fill="none" stroke-linejoin="round">
      <path d="M20 26 V94 M33 26 V94"/>
      <path d="M20 36 L33 46 L20 56 L33 66 L20 76 L33 86 M33 36 L20 46 L33 56 L20 66 L33 76 L20 86"/>
      <path d="M2 25 H99 M14 33 H99 V25"/>
      <path d="M33 33 L41 25 L49 33 L57 25 L65 33 L73 25 L81 33 L89 25 L97 33"/>
      <path d="M22 25 L31 5 L34 25 M31 5 L2 22 M31 5 L99 24"/>
    </g>
    <rect x="1" y="23" width="12" height="12" fill="${INK}"/>
    <rect x="15" y="91" width="24" height="5" fill="${INK}"/>
    <line x1="77" y1="33" x2="77" y2="48" stroke="${INK}" stroke-width="2.6"/>
    <circle cx="77" cy="52" r="3.6" stroke="${INK}" stroke-width="2.6" fill="none"/>
    <path d="M77 56 L66 71 M77 56 L88 71" stroke="${INK}" stroke-width="2.6"/>
    <rect x="57" y="71" width="41" height="14" fill="${INK}"/>`,
};
function iconSVG(name, size = 120) {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="${size}" height="${size}" role="img" aria-label="${name}">${ICONS[name]}</svg>`;
}
if (typeof module !== 'undefined') module.exports = { ICONS, iconSVG };
