/**
 * common/format.js
 */
function pad2(n) { return n < 10 ? '0' + n : '' + n; }

function fmtDate(ts) {
  if (!ts) return '--';
  const d = new Date(ts);
  return `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())} ${pad2(d.getHours())}:${pad2(d.getMinutes())}:${pad2(d.getSeconds())}`;
}

function fmtDateShort(ts) {
  if (!ts) return '--';
  const d = new Date(ts);
  return `${pad2(d.getMonth() + 1)}-${pad2(d.getDate())} ${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
}

function relTime(ts) {
  if (!ts) return '';
  const delta = Math.floor((Date.now() - ts) / 1000);
  if (delta < 60)   return delta + ' 秒前';
  if (delta < 3600) return Math.floor(delta / 60) + ' 分钟前';
  if (delta < 86400) return Math.floor(delta / 3600) + ' 小时前';
  return Math.floor(delta / 86400) + ' 天前';
}

export default { fmtDate, fmtDateShort, relTime };
