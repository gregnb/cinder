import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const scriptDir = path.dirname(fileURLToPath(import.meta.url))
const repoRoot = path.resolve(scriptDir, '..')
const publicDir = path.join(repoRoot, 'www', 'public')
const configPath = path.join(repoRoot, 'www', 'site.config.mjs')

const { siteUrl, sitemapRoutes } = await import(pathToFileURL(configPath).href)

if (!siteUrl || !Array.isArray(sitemapRoutes) || sitemapRoutes.length === 0) {
  throw new Error('www/site.config.mjs must export siteUrl and a non-empty sitemapRoutes array')
}

const normalizedSiteUrl = siteUrl.replace(/\/+$/, '')
const generatedDate = new Date().toISOString().slice(0, 10)

function xmlEscape(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&apos;')
}

function absoluteUrl(routePath) {
  const normalizedPath = routePath === '/' ? '' : `/${routePath.replace(/^\/+/, '').replace(/\/+$/, '')}`
  return `${normalizedSiteUrl}${normalizedPath}`
}

const urls = sitemapRoutes.map((route) => {
  if (!route.path) {
    throw new Error(`Sitemap route is missing path: ${JSON.stringify(route)}`)
  }

  return [
    '  <url>',
    `    <loc>${xmlEscape(absoluteUrl(route.path))}</loc>`,
    `    <lastmod>${xmlEscape(route.lastmod ?? generatedDate)}</lastmod>`,
    route.changefreq ? `    <changefreq>${xmlEscape(route.changefreq)}</changefreq>` : null,
    route.priority ? `    <priority>${xmlEscape(route.priority)}</priority>` : null,
    '  </url>',
  ].filter(Boolean).join('\n')
})

const sitemapXml = [
  '<?xml version="1.0" encoding="UTF-8"?>',
  '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
  urls.join('\n'),
  '</urlset>',
  '',
].join('\n')

const robotsTxt = [
  'User-agent: *',
  'Allow: /',
  '',
  `Sitemap: ${normalizedSiteUrl}/sitemap.xml`,
  '',
].join('\n')

fs.mkdirSync(publicDir, { recursive: true })
fs.writeFileSync(path.join(publicDir, 'sitemap.xml'), sitemapXml)
fs.writeFileSync(path.join(publicDir, 'robots.txt'), robotsTxt)

console.log(`Generated sitemap.xml with ${sitemapRoutes.length} URLs`)
console.log(`Generated robots.txt for ${normalizedSiteUrl}`)
