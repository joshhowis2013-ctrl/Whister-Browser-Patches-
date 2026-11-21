const manifestKey = "win/beta/manifest.json";
const installerPattern = /^win\/beta\/WhisterSetup-\d+\.\d+\.\d+\.\d+\.exe$/;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Method not allowed", { status: 405 });
    }

    const key = url.pathname.replace(/^\/+/, "");
    if (key === "") {
      return indexResponse(request, env);
    }

    if (key !== manifestKey && !installerPattern.test(key)) {
      return new Response("Not found", { status: 404 });
    }

    const object = await env.UPDATES.get(key);
    if (object === null) {
      return new Response("Not found", { status: 404 });
    }

    const headers = new Headers();
    object.writeHttpMetadata(headers);
    headers.set("ETag", object.httpEtag);
    headers.set(
      "Cache-Control",
      key === manifestKey ? "no-store" : "public, max-age=31536000, immutable",
    );
    headers.set(
      "Content-Type",
      key === manifestKey ? "application/json; charset=utf-8" : "application/vnd.microsoft.portable-executable",
    );
    return new Response(request.method === "HEAD" ? null : object.body, { headers });
  },
};

async function indexResponse(request, env) {
  const listing = await env.UPDATES.list({ prefix: "win/beta/" });
  const objects = listing.objects
    .filter(({ key }) => key === manifestKey || installerPattern.test(key))
    .sort((left, right) => right.key.localeCompare(left.key));

  const rows = objects
    .map(
      ({ key, size, uploaded }) =>
        `<tr><td><a href="/${key}">${key}</a></td><td>${formatBytes(size)}</td><td>${uploaded.toISOString()}</td></tr>`,
    )
    .join("");

  const html = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Whister Browser Beta Updates</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 2rem; color: #202124; }
    table { border-collapse: collapse; width: 100%; max-width: 60rem; }
    th, td { border-bottom: 1px solid #dadce0; padding: .75rem; text-align: left; }
    th { background: #f8f9fa; }
  </style>
</head>
<body>
  <h1>Whister Browser Beta Updates</h1>
  <table>
    <thead><tr><th>File</th><th>Size</th><th>Uploaded</th></tr></thead>
    <tbody>${rows || "<tr><td colspan=\"3\">No beta updates have been published.</td></tr>"}</tbody>
  </table>
</body>
</html>`;

  return new Response(request.method === "HEAD" ? null : html, {
    headers: {
      "Cache-Control": "no-store",
      "Content-Type": "text/html; charset=utf-8",
    },
  });
}

function formatBytes(bytes) {
  if (bytes < 1024) {
    return `${bytes} B`;
  }

  const units = ["KB", "MB", "GB"];
  const exponent = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length);
  return `${(bytes / 1024 ** exponent).toFixed(2)} ${units[exponent - 1]}`;
}
