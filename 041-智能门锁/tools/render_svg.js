// 通用 SVG → PNG 渲染（中文用 msyh.ttc，避免方框/回退字体）
// 用法: node render_svg.js <input.svg> <output.png> [zoom]
const { Resvg } = require('@resvg/resvg-js');
const fs = require('fs');

const [inp, out, zoomArg] = process.argv.slice(2);
if (!inp || !out) {
  console.error('usage: node render_svg.js <input.svg> <output.png> [zoom]');
  process.exit(1);
}
const zoom = parseFloat(zoomArg || '1.5');
const svg = fs.readFileSync(inp, 'utf8');
const resvg = new Resvg(svg, {
  font: { fontFiles: ['C:/Windows/Fonts/msyh.ttc'], loadSystemFonts: false },
  fitTo: { mode: 'zoom', value: zoom },
});
const png = resvg.render();
fs.writeFileSync(out, png.asPng());
console.log('OK', out, png.width + 'x' + png.height,
            Math.round(png.asPng().length / 1024) + 'KB');
