# Notices Channel (`avisos.json`)

The app reads `avisos.json` from this repository (branch `master`, via
raw.githubusercontent.com) every 30 minutes while the app is open and displays each
entry in the notification center (toast + list; BLUE on LG).
It serves to notify users of known issues while a fix is pending, or
anything worth communicating to the person sitting in front of the TV.

```json
[
  {
    "id": "2026-09-19-guia-lento",
    "desde": "2026-09-19",
    "ate": "2026-10-01",
    "plataforma": "todas",
    "ate_versao": "1.3.1",
    "titulo": "Guia de TV demorando",
    "titulo_en": "TV Guide slow to open",
    "texto": "Na 1.3.1 o guia espera a rede a cada abertura. A 1.3.2 corrige.",
    "texto_en": "On 1.3.1 the guide waits for the network on every open. 1.3.2 fixes it."
  }
]
```

- `id`: unique and stable. This acts as the "already seen" mark: changing the id causes
  the notice to reappear for everyone.
- `desde` / `ate`: ISO dates; outside this range the notice does not appear. Can be omitted.
- `plataforma`: `todas` (all) or `lg`.
- `ate_versao`: only appears for users on this version or older. Can be omitted.
- `titulo_en` / `texto_en`: optional English title and text.

A notice removed from the file disappears from the list on the next refresh.
