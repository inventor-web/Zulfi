// Service worker mínimo: só o necessário para o navegador considerar
// o app "instalável" e abrir em modo standalone (sem barra de endereço).
// Não faz cache agressivo porque o app sempre precisa de rede
// (Wi-Fi ou Bluetooth) para falar com o ESP32 — não faz sentido funcionar
// "offline" de verdade.

const CACHE_NAME = "lampada-shell-v1";
const SHELL_FILES = [
  "./controle_lampada.html",
  "./manifest.json",
  "./icon-192.png",
  "./icon-512.png",
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(SHELL_FILES))
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(
        keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k))
      )
    )
  );
  self.clients.claim();
});

// Serve a "casca" do app (HTML/ícones/manifest) do cache quando não há rede,
// mas deixa qualquer outra requisição (chamadas ao ESP32) ir direto pra rede.
self.addEventListener("fetch", (event) => {
  const url = new URL(event.request.url);
  const isShellFile = SHELL_FILES.some((f) => url.pathname.endsWith(f.replace("./", "")));

  if (isShellFile) {
    event.respondWith(
      caches.match(event.request).then((cached) => cached || fetch(event.request))
    );
  }
});
