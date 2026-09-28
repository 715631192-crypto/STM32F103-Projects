const { Resvg } = require('@resvg/resvg-js');
const fs = require('fs');
const path = require('path');

const root = 'C:/Users/71563/Desktop/block/docs';
const svg = fs.readFileSync(path.join(root, 'task_queue_architecture.svg'), 'utf8');
const resvg = new Resvg(svg, {
  font: { fontFiles: ['C:/Windows/Fonts/msyh.ttc'], loadSystemFonts: false },
  fitTo: { mode: 'zoom', value: 2.0 }
});
const png = resvg.render();
fs.writeFileSync(path.join(root, 'task_queue_architecture.png'), png.asPng());
console.log('OK', png.width + 'x' + png.height);
