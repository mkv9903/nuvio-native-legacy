/**
 * Deliberately small, in-memory model for the settings UX prototype.
 * Values and defaults describe the demo, not a device capability assessment.
 * Nothing here writes app preferences, contacts a service or starts a timer.
 */

function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

const choice = (value, label, description) => ({ value, label, description });
const toggle = (on, off) => [
  choice('on', 'On', on),
  choice('off', 'Off', off),
];

export const SECTIONS = freeze([
  { id: 'trailers', label: 'Trailers', description: 'Choose where trailers appear and when they have sound.' },
  { id: 'languages', label: 'Languages and subtitles', description: 'Interface, audio, subtitles, and title texts have independent choices.' },
  { id: 'performance', label: 'Performance on this TV', description: 'Adjust interface and images for this device.' },
]);

export const OPTIONS = freeze([
  {
    id: 'heroTrailer', section: 'trailers', group: 'Where to play',
    label: 'Top hero banner',
    description: 'When focus stays on top hero, an available trailer can replace the image. Off keeps artwork.',
    scope: 'profile', defaultValue: 'on',
    values: toggle('Shows trailer in place of hero artwork when available.', 'Keeps hero artwork.'),
    advanced: false, synonyms: ['trailer', 'hero', 'automatic', 'autoplay', 'background'], preview: 'trailer',
  },
  {
    id: 'focusTrailer', section: 'trailers', group: 'Where to play',
    label: 'Focused poster',
    description: 'When pausing on a poster, shows a muted trailer. In app, requires expanded or horizontal poster; safe mode may suspend effect.',
    scope: 'profile', defaultValue: 'off',
    values: toggle('Shows muted trailer after poster delay.', 'Keeps artwork when pausing on a poster.'),
    advanced: false, synonyms: ['trailer', 'poster', 'focus', 'autoplay'], preview: 'trailer',
  },
  {
    id: 'detailTrailer', section: 'trailers', group: 'Where to play',
    label: 'Title details page',
    description: 'An available trailer can begin upon opening a title. Off keeps background artwork.',
    scope: 'profile', defaultValue: 'on',
    values: toggle('Starts available trailer on title details page.', 'Keeps background artwork on title details page.'),
    advanced: false, synonyms: ['trailer', 'details', 'movie', 'series', 'autoplay'], preview: 'trailer',
  },
  {
    id: 'heroSound', section: 'trailers', group: 'Sound and image',
    label: 'Sound on top hero',
    description: 'Chooses whether top hero trailer plays with sound. In app, audio also depends on TV and trailer source.',
    scope: 'device', defaultValue: 'off',
    values: toggle('Allows audio on hero trailer when TV and source support it.', 'Keeps hero trailer muted.'),
    advanced: false, synonyms: ['trailer', 'mute', 'silence', 'audio', 'volume'], preview: 'trailer', requires: 'heroTrailer',
  },
  {
    id: 'detailSound', section: 'trailers', group: 'Sound and image',
    label: 'Sound on title details page',
    description: 'Chooses audio for auto trailer on title details page. In app, TV and source must provide audio.',
    scope: 'device', defaultValue: 'off',
    values: toggle('Allows audio on auto trailer when supported.', 'Keeps auto trailer muted.'),
    advanced: false, synonyms: ['trailer', 'details', 'mute', 'audio', 'volume'], preview: 'trailer', requires: 'detailTrailer',
  },
  {
    id: 'trailerSource', section: 'trailers', group: 'Sound and image',
    label: 'Trailer source',
    description: 'Automatic searches across compatible sources. Selected source restricts search; availability and audio vary by TV.',
    scope: 'device', defaultValue: 'auto',
    values: [
      choice('auto', 'Automatic', 'Uses compatible source order for TV.'),
      choice('apple', 'Apple TV', 'Searches only Apple TV trailers.'),
      choice('imdb', 'IMDb', 'Searches only IMDb trailers.'),
    ],
    advanced: false, synonyms: ['trailer', 'provider', 'source', 'apple', 'imdb'], preview: 'trailer',
  },
  {
    id: 'trailerDelay', section: 'trailers', group: 'Sound and image',
    label: 'Delay on top hero',
    description: 'Dwell time on hero before replacing artwork with trailer. Not the poster dwell time.',
    scope: 'device', defaultValue: '5',
    values: [
      choice('3', '3 seconds', 'Starts earlier when focus remains on hero.'),
      choice('5', '5 seconds', 'Provides time to view artwork before trailer.'),
      choice('8', '8 seconds', 'Waits longer before displaying trailer.'),
    ],
    advanced: true, synonyms: ['trailer', 'delay', 'time', 'seconds'], preview: 'trailer', requires: 'heroTrailer',
  },
  {
    id: 'trailerQuality', section: 'trailers', group: 'Sound and image',
    label: 'Trailer quality',
    description: 'Limits trailer video definition. Source may deliver lower resolution than chosen.',
    scope: 'profile', defaultValue: 'max',
    values: [
      choice('720', 'Up to 720p', 'Requests smaller trailers when source allows.'),
      choice('1080', 'Up to 1080p', 'Limits trailer to Full HD.'),
      choice('max', 'Maximum available', 'Uses highest available resolution for trailer.'),
    ],
    advanced: true, synonyms: ['trailer', 'resolution', 'definition', 'network', 'internet'], preview: 'trailer',
  },
  {
    id: 'uiLang', section: 'languages', group: 'Each language has its place',
    label: 'Interface language',
    description: 'Changes menus and buttons on this TV. Audio, subtitles, and title texts retain their own settings.',
    scope: 'device', defaultValue: 'auto',
    values: [
      choice('auto', 'Automatic', 'In app, follows account, falling back to TV language.'),
      choice('pt', 'Português', 'Menus and buttons in Portuguese.'),
      choice('en', 'English', 'Menus and buttons in English.'),
      choice('es', 'Español', 'Menus and buttons in Spanish.'),
    ],
    advanced: false, synonyms: ['language', 'menu', 'portuguese', 'english', 'spanish'], preview: 'language',
  },
  {
    id: 'audioLang', section: 'languages', group: 'Each language has its place',
    label: 'Preferred audio language',
    description: 'Guides track selection when video has multiple audio tracks. Does not create dub if track does not exist.',
    scope: 'profile', defaultValue: 'original',
    values: [
      choice('original', 'Original', 'Does not prioritize dubbed track; uses available original audio track.'),
      choice('pt', 'Português', 'Prefers Portuguese audio when track exists.'),
      choice('en', 'English', 'Prefers English audio when track exists.'),
      choice('es', 'Español', 'Prefers Spanish audio when track exists.'),
    ],
    advanced: false, synonyms: ['dubbed', 'dub', 'voice', 'sound', 'language', 'audio'], preview: 'language',
  },
  {
    id: 'subtitleLang', section: 'languages', group: 'Each language has its place',
    label: 'Preferred subtitle language',
    description: 'Chooses preferred subtitle language for subsequent playbacks. Track must be available.',
    scope: 'profile', defaultValue: 'en',
    values: [
      choice('account', 'From account', 'Follows profile subtitle preference when known.'),
      choice('pt', 'Português', 'Prefers Portuguese subtitles.'),
      choice('en', 'English', 'Prefers English subtitles.'),
      choice('es', 'Español', 'Prefers Spanish subtitles.'),
    ],
    advanced: false, synonyms: ['subtitle', 'subtitles', 'cc', 'text'], preview: 'language',
  },
  {
    id: 'metadataLang', section: 'languages', group: 'Each language has its place',
    label: 'Titles and synopsis language',
    description: 'Guides metadata received from TMDB. Untranslated texts may remain in available language; audio and subtitles do not change.',
    scope: 'profile', defaultValue: 'interface',
    values: [
      choice('interface', 'From interface', 'Requests texts in interface language.'),
      choice('pt', 'Português', 'Requests titles and synopses in Portuguese when available.'),
      choice('en', 'English', 'Requests titles and synopses in English when available.'),
      choice('es', 'Español', 'Requests titles and synopses in Spanish when available.'),
    ],
    advanced: false, synonyms: ['metadata', 'tmdb', 'synopsis', 'name', 'title', 'translation'], preview: 'language',
  },
  {
    id: 'resolution', section: 'performance', group: 'Interface and images',
    label: 'Interface resolution',
    description: 'Changes menus and buttons definition. Movie quality does not change; in app, resolution depends on TV and may require restart.',
    scope: 'device', defaultValue: '1080',
    values: [
      choice('720', '720p · lighter', 'Draws menus in lower resolution; text may appear softer.'),
      choice('1080', '1080p · Full HD', 'Draws interface in Full HD.'),
      choice('2160', '2160p · 4K', 'Requests interface in 4K; TV may stay at 1080p.'),
    ],
    advanced: false, synonyms: ['4k', '720p', '1080p', 'definition', 'slow', 'stutter'], preview: 'performance',
  },
  {
    id: 'imageQuality', section: 'performance', group: 'Interface and images',
    label: 'Image quality',
    description: 'Changes size of loaded artwork. Smaller images use less memory; does not change video quality.',
    scope: 'device', defaultValue: 'medium',
    values: [
      choice('low', 'Low', 'Loads smaller artwork and uses less memory.'),
      choice('medium', 'Medium', 'Uses intermediate size for posters and backgrounds.'),
      choice('high', 'High', 'Loads larger artwork and uses more memory.'),
    ],
    advanced: false, synonyms: ['poster', 'art', 'ram', 'slow', 'stutter'], preview: 'performance',
  },
  {
    id: 'glass', section: 'performance', group: 'Interface and images',
    label: 'Glass on panels',
    description: 'Creates translucent panels over background. Off uses solid surfaces and reduces image composition workload.',
    scope: 'device', defaultValue: 'on',
    values: toggle('Uses translucent panels over artwork.', 'Uses solid panels with fewer composition effects.'),
    advanced: false, synonyms: ['transparency', 'blur', 'slow', 'stutter', 'gpu'], preview: 'performance',
  },
  {
    id: 'gpuEffects', section: 'performance', group: 'Interface and images',
    label: 'Interface effects',
    description: 'Controls blur and gloss on platforms offering this option. Sample does not measure TV GPU.',
    scope: 'device', defaultValue: 'auto',
    values: [
      choice('auto', 'Automatic', 'In app, allows reducing effects according to TV metrics.'),
      choice('light', 'Light', 'Reduces blur and gloss.'),
      choice('full', 'Full', 'Keeps available effects on platform.'),
    ],
    advanced: false, synonyms: ['gpu', 'animation', 'gloss', 'blur', 'slow', 'stutter'], preview: 'performance',
  },
  {
    id: 'rowLimit', section: 'performance', group: 'Home screen load',
    label: 'Row limit',
    description: 'Limits how many rows Home screen builds. Fewer rows also mean fewer catalog requests and less memory.',
    scope: 'device', defaultValue: '7',
    values: [
      choice('5', '5 rows', 'Builds up to five rows on Home screen.'),
      choice('7', '7 rows', 'Builds up to seven rows on Home screen.'),
      choice('12', '12 rows', 'Builds up to twelve rows, with higher memory and network usage.'),
      choice('16', '16 rows', 'Builds up to sixteen rows, with higher memory and network usage.'),
    ],
    advanced: false, synonyms: ['home', 'catalog', 'row', 'slow', 'stutter'], preview: 'performance',
  },
  {
    id: 'textureMemory', section: 'performance', group: 'Home screen load',
    label: 'Texture memory for images',
    description: 'Sets image cache budget ceiling. Automatic considers RAM; in app, requested value may be capped by device.',
    scope: 'device', defaultValue: 'auto',
    values: [
      choice('auto', 'Automatic', 'Lets app dimension cache for device.'),
      choice('64', '64 MB', 'Requests up to 64 MB for image cache.'),
      choice('128', '128 MB', 'Requests up to 128 MB, respecting TV limit.'),
      choice('256', '256 MB', 'Requests up to 256 MB, respecting TV limit.'),
    ],
    advanced: true, synonyms: ['texture', 'cache', 'ram', 'megabytes', 'stutter', 'memory'], preview: 'performance',
  },
]);

const optionsById = new Map(OPTIONS.map(option => [option.id, option]));

export const DEFAULTS = freeze(Object.fromEntries(OPTIONS.map(option => [option.id, option.defaultValue])));
export const DEMO_VALUES = freeze({ ...DEFAULTS, focusTrailer: 'on', uiLang: 'pt' });

export const RECOMMENDATION = freeze({
  label: 'Try a lighter interface',
  description: 'Sample preset for this TV: solid panels, smaller images, and up to seven rows. Not a device diagnostics run.',
  changes: { glass: 'off', imageQuality: 'low', rowLimit: '7' },
});

export function getOption(id) {
  const option = optionsById.get(id);
  if (!option) throw new RangeError(`Unknown setting: ${String(id)}`);
  return option;
}

export function valueLabel(id, value) {
  const option = getOption(id);
  const item = option.values.find(item => item.value === value);
  if (!item) throw new RangeError(`Invalid value for ${id}: ${String(value)}`);
  return item.label;
}

const normalize = text => String(text ?? '').normalize('NFD').replace(/\p{M}/gu, '').toLowerCase().trim();

/** Empty searches are handled as local topic suggestions by the view. */
export function searchOptions(query) {
  const needle = normalize(query).replace(/\s+/g, ' ');
  if (!needle) return [];
  const words = needle.split(' ');
  return OPTIONS.map((option, index) => {
    const label = normalize(option.label);
    const synonyms = option.synonyms.map(normalize);
    const section = SECTIONS.find(section => section.id === option.section);
    const content = normalize([
      option.label, option.description, option.group, section.label,
      ...option.synonyms,
      ...option.values.flatMap(item => [item.label, item.description]),
    ].join(' '));
    if (!words.every(word => content.includes(word))) return null;
    const score = label === needle ? 1000
      : label.includes(needle) ? 800
      : synonyms.includes(needle) ? 600
      : synonyms.some(word => word.includes(needle)) ? 500
      : words.every(word => label.includes(word)) ? 400
      : 100;
    return { option, index, score };
  }).filter(Boolean).sort((a, b) => b.score - a.score || a.index - b.index).map(result => result.option);
}

export function changedOptions(values) {
  return OPTIONS.filter(option => (values[option.id] ?? option.defaultValue) !== option.defaultValue);
}

export function isAvailable(option, values) {
  const resolved = getOption(typeof option === 'string' ? option : option.id);
  if (resolved.requires && (values[resolved.requires] ?? DEFAULTS[resolved.requires]) !== 'on') {
    return {
      available: false,
      reason: `Enable “${getOption(resolved.requires).label}” to change this option.`,
      requires: resolved.requires,
    };
  }
  return { available: true, reason: '' };
}

function validate(changes) {
  if (!changes || typeof changes !== 'object' || Array.isArray(changes)) {
    throw new TypeError('Changes must be a map of settings and values.');
  }
  for (const [id, value] of Object.entries(changes)) {
    const option = getOption(id);
    if (typeof value !== 'string' || !option.values.some(item => item.value === value)) {
      throw new RangeError(`Invalid value for ${id}: ${String(value)}`);
    }
  }
  return { ...changes };
}

/** Saved state changes only on commit or explicit demo reset. */
export function createModel(initial = DEMO_VALUES) {
  let saved = { ...DEFAULTS, ...validate(initial) };
  let draft = null;
  const copyDraft = () => draft ? { changes: { ...draft.changes }, previous: { ...draft.previous } } : null;

  return {
    get saved() { return { ...saved }; },
    get draft() { return copyDraft(); },
    begin(changes = {}) {
      const valid = validate(changes);
      draft = { changes: valid, previous: { ...saved } };
      return copyDraft();
    },
    candidate(id, value) {
      if (!draft) throw new Error('Open an edit session before choosing a value.');
      const valid = validate({ [id]: value });
      draft.changes = { ...draft.changes, ...valid };
      return copyDraft();
    },
    previewValues() {
      return { ...saved, ...(draft?.changes ?? {}) };
    },
    commit() {
      if (!draft) return [];
      const changed = OPTIONS.filter(option => Object.hasOwn(draft.changes, option.id)
        && saved[option.id] !== draft.changes[option.id]).map(option => option.id);
      saved = { ...saved, ...draft.changes };
      draft = null;
      return changed;
    },
    cancel() {
      draft = null;
      return { ...saved };
    },
    reset() {
      const changed = changedOptions(saved).map(option => option.id);
      saved = { ...DEFAULTS };
      draft = null;
      return changed;
    },
    snapshot() {
      return { saved: { ...saved }, draft: copyDraft() };
    },
  };
}
