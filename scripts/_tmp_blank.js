const fs = require("fs");
const p = "E:/code/wumingkeji/Metalio-E-INK4/main/cloudzao_endpoints.c";
let t = fs.readFileSync(p, "utf8");
const nonempty = (t.match(/=\s*"[^"]+"/g) || []).length;
console.log("nonempty_before=", nonempty);
t = t.replace(/=\s*"[^"]*"/g, '= ""');
fs.writeFileSync(p, t);
console.log("blanked_ok");
