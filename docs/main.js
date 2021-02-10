(()=>{"use strict";var e={386:(e,t,n)=>{Object.defineProperty(t,"__esModule",{value:!0}),t.DomUtil=void 0;const r=n(507);class s{static setStyles(e,t){Object.assign(e.style,t)}static setMouseDragListener(e,t,n){let r=null,s=null;if("object"==typeof e){const i=e;e=i.move,t=i.up,r=i.leave,n=i.useCapture,s=null==r?null:i.leaveTarget||document}const i=()=>{document.removeEventListener("mousemove",e,n),document.removeEventListener("mouseup",o,n),document.removeEventListener("touchmove",e,n),document.removeEventListener("touchend",o,n),a&&s&&(s.removeEventListener("mouseleave",a,n),s.removeEventListener("touchcancel",a,n))},o=e=>{t&&t(e),i()},a=null==r?null:e=>{r&&r(e)&&i()};document.addEventListener("mousemove",e,n),document.addEventListener("mouseup",o,n),document.addEventListener("touchmove",e,n),document.addEventListener("touchend",o,n),a&&s&&(s.addEventListener("mouseleave",a,n),s.addEventListener("touchcancel",a,n))}static getMousePosIn(e,t){const n=t.getBoundingClientRect(),r=document.body.scrollLeft,s=document.body.scrollTop,i=e.changedTouches;let o,a;if(i){const e=i[0];if(0!==e.identifier)return null;o=e.clientX,a=e.clientY}else{const t=e;o=t.pageX,a=t.pageY}return[o-n.left-r,a-n.top-s]}static createCanvas(e,t){const n=document.createElement("canvas");return n.width=e,n.height=t,n}static makeDraggable(e){const t=t=>{if("mousedown"===t.type&&0!==t.button)return!1;const n=document.body.getBoundingClientRect();t.preventDefault();const[i,o]=s.getMousePosIn(t,e),a=-i,c=-o,l=e.getBoundingClientRect(),d=l.width,u=l.height,h={x:i,y:o};return s.setMouseDragListener({move:t=>{const i=s.getMousePosIn(t,e.parentNode);null!=i&&(h.x=r.Util.clamp(i[0]+a,0,Math.floor(n.width-d)),h.y=r.Util.clamp(i[1]+c,0,Math.floor(n.height-u)),s.setStyles(e,{left:`${Math.round(h.x)}px`,top:`${Math.round(h.y)}px`}))},up:e=>{}}),!0};e.addEventListener("mousedown",t),e.addEventListener("touchstart",t)}static addDivButton(e,t){const n=document.createElement("div");return s.setStyles(n,{position:"absolute",right:0,top:0}),n.addEventListener("click",t),n.addEventListener("mousedown",(e=>0===e.button&&(e.stopPropagation(),!0))),n.addEventListener("touchstart",t),e.appendChild(n),n}static putOnCenter(e){const t=document.body.getBoundingClientRect(),n=e.getBoundingClientRect(),i=n.width,o=n.height,a=r.Util.clamp(.5*(t.width-i),0,Math.floor(t.width-i)),c=r.Util.clamp(.5*(t.height-o),0,Math.floor(t.height-o));s.setStyles(e,{left:`${Math.round(a)}px`,top:`${Math.round(c)}px`})}}t.DomUtil=s},110:(e,t)=>{Object.defineProperty(t,"__esModule",{value:!0}),t.ExampleCodes=void 0,t.ExampleCodes={hello:'#include <stdio.h>\n\nint main(void) {\n  printf("Hello, world!\\n");\n  return 0;\n}',sieve:'#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n\nvoid sieve(int n) {\n  uint8_t *notprime = calloc(sizeof(*notprime), n);\n  if (notprime == NULL) {\n    fprintf(stderr, "malloc failed\\n");\n    exit(1);\n  }\n\n  for (int i = 2; i < n; ++i) {\n    if (notprime[i])\n      continue;\n    printf("%d\\n", i);\n    for (int j = i * i; j < n; j += i)\n      notprime[j] = true;\n  }\n  free(notprime);\n}\n\nint main(int argc, char *argv[]) {\n  int n = 100;\n  if (argc > 1) {\n    n = atoi(argv[1]);\n    if (n < 2) {\n      fprintf(stderr, "should be >= 2\\n");\n      return 1;\n    }\n  }\n  sieve(n);\n  return 0;\n}',qsort:'#include <stdio.h>\n#include <stdlib.h>\n\nint compare(const void *va, const void *vb) {\n  const int *pa = va;\n  const int *pb = vb;\n  return *pa - *pb;\n}\n\nint main(int argc, char *argv[]) {\n  int array[] = {5, 9, 3, 8, 4, 0, 7, 1, 6, 2};\n\n  qsort(array, sizeof(array) / sizeof(*array), sizeof(*array), compare);\n\n  for (int i = 0; i < sizeof(array) / sizeof(*array); ++i)\n    printf("%d ", array[i]);\n  printf("\\n");\n  return 0;\n}',aobench:"#include <math.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n#define WIDTH        (256)\n#define HEIGHT       (256)\n#define NSUBSAMPLES  (2)\n#define NAO_SAMPLES  (8)\n\ntypedef struct {double x, y, z;} vec;\n\nstatic double vdot(const vec *v0, const vec *v1) {\n  return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;\n}\n\nstatic void vcross(vec *c, const vec *v0, const vec *v1) {\n  c->x = v0->y * v1->z - v0->z * v1->y;\n  c->y = v0->z * v1->x - v0->x * v1->z;\n  c->z = v0->x * v1->y - v0->y * v1->x;\n}\n\nstatic void vnormalize(vec *c) {\n  double length = sqrt(vdot(c, c));\n  if (length > 1.0e-17) {\n    c->x /= length;\n    c->y /= length;\n    c->z /= length;\n  }\n}\n\ndouble EPS = 1.0e-6;\n\ntypedef struct {\n  double t;\n  vec p;\n  vec n;\n} Isect;\n\ntypedef struct {\n  vec center;\n  double radius;\n} Sphere;\n\ntypedef struct {\n  vec p;\n  vec n;\n} Plane;\n\ntypedef struct {\n  vec org;\n  vec dir;\n} Ray;\n\nvoid ray_sphere_intersect(Isect *isect, const Ray *ray, const Sphere *sphere) {\n  vec rs;\n  rs.x = ray->org.x - sphere->center.x;\n  rs.y = ray->org.y - sphere->center.y;\n  rs.z = ray->org.z - sphere->center.z;\n\n  double B = vdot(&rs, &ray->dir);\n  double C = vdot(&rs, &rs) - sphere->radius * sphere->radius;\n  double D = B * B - C;\n  if (D > 0.0) {\n    double t = -B - sqrt(D);\n    if (t > EPS && t < isect->t) {\n      isect->t = t;\n\n      isect->p.x = ray->org.x + ray->dir.x * t;\n      isect->p.y = ray->org.y + ray->dir.y * t;\n      isect->p.z = ray->org.z + ray->dir.z * t;\n\n      isect->n.x = isect->p.x - sphere->center.x;\n      isect->n.y = isect->p.y - sphere->center.y;\n      isect->n.z = isect->p.z - sphere->center.z;\n      vnormalize(&isect->n);\n    }\n  }\n}\n\nvoid ray_plane_intersect(Isect *isect, const Ray *ray, const Plane *plane) {\n  double d = -vdot(&plane->p, &plane->n);\n  double v = vdot(&ray->dir, &plane->n);\n\n  if (fabs(v) < EPS)\n    return;\n\n  double t = -(vdot(&ray->org, &plane->n) + d) / v;\n  if (t > EPS && t < isect->t) {\n    isect->t = t;\n\n    isect->p.x = ray->org.x + ray->dir.x * t;\n    isect->p.y = ray->org.y + ray->dir.y * t;\n    isect->p.z = ray->org.z + ray->dir.z * t;\n\n    isect->n = plane->n;\n  }\n}\n\ndouble copysign(double x, double f) {\n  return f >= 0 ? x : -x;\n}\n\nvoid orthoBasis(vec *basis, const vec *n) {\n  basis[2] = *n;\n  double sign = copysign(1.0, n->z);\n  const double a = -1.0 / (sign + n->z);\n  const double b = n->x * n->y * a;\n  basis[0].x = 1.0 + sign * n->x * n->x * a;\n  basis[0].y = sign * b;\n  basis[0].z = -sign * n->x;\n  basis[1].x = b;\n  basis[1].y = sign + n->y * n->y * a;\n  basis[1].z = -n->y;\n}\n\nconst Sphere spheres[3] = {\n  {{-2.0, 0, -3.5},  0.5},\n  {{-0.5, 0, -3.0},  0.5},\n  {{ 1.0, 0, -2.2},  0.5},\n};\n\nconst Plane plane = {\n  {0.0, -0.5, 0.0},\n  {0.0,  1.0, 0.0},\n};\n\nvoid ambient_occlusion(vec *col, const Isect *isect) {\n  int ntheta = NAO_SAMPLES;\n  int nphi   = NAO_SAMPLES;\n\n  vec basis[3];\n  orthoBasis(basis, &isect->n);\n\n  int occlusion = 0;\n  for (int j = 0; j < ntheta; ++j) {\n    for (int i = 0; i < nphi; ++i) {\n      double theta = sqrt(drand48());\n      double phi   = 2.0 * M_PI * drand48();\n\n      double x = cos(phi) * theta;\n      double y = sin(phi) * theta;\n      double z = sqrt(1.0 - theta * theta);\n\n      // local -> global\n      double rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;\n      double ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;\n      double rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;\n\n      Ray ray;\n      ray.org = isect->p;\n      ray.dir.x = rx;\n      ray.dir.y = ry;\n      ray.dir.z = rz;\n\n      Isect occIsect;\n      occIsect.t   = HUGE_VAL;\n\n      ray_sphere_intersect(&occIsect, &ray, &spheres[0]);\n      ray_sphere_intersect(&occIsect, &ray, &spheres[1]);\n      ray_sphere_intersect(&occIsect, &ray, &spheres[2]);\n      ray_plane_intersect (&occIsect, &ray, &plane);\n\n      if (occIsect.t < HUGE_VAL)\n        ++occlusion;\n    }\n  }\n\n  double c = (ntheta * nphi - occlusion) / (double)(ntheta * nphi);\n  col->x = col->y = col->z = c;\n}\n\nunsigned char clamp(double f) {\n  int i = (int)(f * 255.5);\n  if (i < 0) i = 0;\n  else if (i > 255) i = 255;\n  return i;\n}\n\nvoid render(unsigned char *img, int w, int h, int nsubsamples) {\n  double coeff = 1.0 / (nsubsamples * nsubsamples);\n  unsigned char *dst = img;\n  for (int y = 0; y < h; ++y) {\n    for (int x = 0; x < w; ++x) {\n      double cr = 0, cg = 0, cb = 0;\n      for (int v = 0; v < nsubsamples; ++v) {\n        for (int u = 0; u < nsubsamples; ++u) {\n          double px =  (x + (u / (double)nsubsamples) - (w / 2.0)) / (w / 2.0);\n          double py = -(y + (v / (double)nsubsamples) - (h / 2.0)) / (h / 2.0);\n\n          Ray ray;\n          ray.org.x = 0.0;\n          ray.org.y = 0.0;\n          ray.org.z = 0.0;\n\n          ray.dir.x = px;\n          ray.dir.y = py;\n          ray.dir.z = -1.0;\n          vnormalize(&ray.dir);\n\n          Isect isect;\n          isect.t = HUGE_VAL;\n\n          ray_sphere_intersect(&isect, &ray, &spheres[0]);\n          ray_sphere_intersect(&isect, &ray, &spheres[1]);\n          ray_sphere_intersect(&isect, &ray, &spheres[2]);\n          ray_plane_intersect (&isect, &ray, &plane);\n\n          if (isect.t < HUGE_VAL) {\n            vec col;\n            ambient_occlusion(&col, &isect);\n\n            cr += col.x;\n            cg += col.y;\n            cb += col.z;\n          }\n        }\n      }\n\n      *dst++ = clamp(cr * coeff);\n      *dst++ = clamp(cg * coeff);\n      *dst++ = clamp(cb * coeff);\n      *dst++ = 255;\n    }\n  }\n}\n\nextern void showGraphic(int width, int height, void *img);\n\nint main(void) {\n  unsigned char *img = malloc(WIDTH * HEIGHT * 4);\n  render(img, WIDTH, HEIGHT, NSUBSAMPLES);\n  showGraphic(WIDTH, HEIGHT, img);\n  return 0;\n}"}},949:(e,t,n)=>{Object.defineProperty(t,"__esModule",{value:!0}),t.FileSystem=t.WaStorage=void 0;const r=n(507),s={0:"r",1:"w",2:"w+",769:"w"};t.WaStorage=class{constructor(){this.files={}}putFile(e,t){"string"==typeof t&&(t=r.Util.encode(t)),console.assert(t.constructor===Uint8Array,t),this.files[e]=t}getFile(e){return this.files[e]}contains(e){return e in this.files}},t.FileSystem=class{constructor(e){this.storage=e,this.fileDescs=["stdin","stdout","stderr"]}saveFile(e,t){this.storage.putFile(e,t)}loadFile(e){return this.storage.getFile(e)}open(e,t,n){if(null==e||0===e.length)return-1;if(0==(3&t)&&!this.storage.contains(e))return-1;null==s[t]&&(console.error(`Unsupported open flag: ${t}`),process.exit(1));const r=this.allocFd(),i={absPath:e,rp:0};return 0!=(3&t)&&(i.write=[],i.writeTotal=0),this.fileDescs[r]=i,r}close(e){return null==this.fileDescs[e]?-1:(this.commitDesc(e),this.fileDescs[e]=null,0)}read(e,t){if(e<0||e>=this.fileDescs.length)return 0;const n=this.fileDescs[e];if(null==n)return 0;const r=null!=n.absPath?this.storage.getFile(n.absPath):n.written;if(null==r||n.rp>=r.length)return 0;const s=Math.min(r.length,n.rp+t.length);t.set(r.subarray(n.rp,s));const i=s-n.rp;return n.rp=s,i}write(e,t){if(e<0||e>=this.fileDescs.length)return 0;if(e<3){const n=r.Util.decodeString(t,0,t.length);return 1!==e&&2!==e||r.Util.putTerminal(n),t.length}const n=this.fileDescs[e];return null==n?0:(n.write.push(t.slice(0)),n.writeTotal+=t.byteLength,t.length)}lseek(e,t,n){const r=this.fileDescs[e];if(null==r)return-1;let s;switch(this.commitDesc(e),n){default:case 0:s=t;break;case 1:s=this.fileDescs[e].position+t;break;case 2:console.assert(!1,"TODO: Implement")}return r.rp=s,s}tmpfile(){const e=this.allocFd();return this.fileDescs[e]={absPath:null,rp:0,write:[],writeTotal:0},e}allocFd(){const e=this.fileDescs.length;for(let t=0;t<e;++t)if(null==this.fileDescs[t])return t;return this.fileDescs.push(null),e}commitDesc(e){if(0<=e&&e<this.fileDescs.length){const t=this.fileDescs[e];if(null!=t&&null!=t.write&&t.write.length>0){const e=new Uint8Array(t.writeTotal);let n=0;for(let r=0;r<t.write.length;++r){const s=t.write[r];new Uint8Array(e.buffer,n,s.byteLength).set(s),n+=s.byteLength}null!=t.absPath?this.saveFile(t.absPath,e):t.written=e,t.write.length=0}}}}},697:(e,t,n)=>{const r=n(386),s=n(507),i=n(695),o=n(949),a=n(110),c="wcc-code",l=new(ace.require("ace/ext/split").Split)(document.getElementById("editor"));l.setOrientation(l.BESIDE),l.setSplits(2),l.setFontSize(16);const d=ace.require("ace/undomanager").UndoManager,u=(()=>{const e=l.getEditor(0);e.$blockScrolling=1/0,e.setOptions({tabSize:2,useSoftTabs:!0,printMarginColumn:!1}),e.setTheme("ace/theme/monokai"),e.getSession().setMode("ace/mode/c_cpp"),e.renderer.$cursorLayer.setBlinking(!0),e.container.classList.add("code-editor");const t=document.getElementById("default-code");t&&e.setValue(t.innerText.trim()+"\n",-1),e.gotoLine(0,0),e.focus();const n=new d;return e.getSession().setUndoManager(n),e.commands.addCommands([{Name:"Undo",bindKey:{win:"Ctrl-Z",mac:"Command-Z"},exec:e=>e.session.getUndoManager().undo()},{Name:"Redo",bindKey:{win:"Ctrl-Shift-Z",mac:"Command-Shift-Z"},exec:e=>e.session.getUndoManager().redo()}]),e})();function h(){return!u.session.getUndoManager().isClean()}function m(e,t){return!(null==e||h()&&!window.confirm(`Buffer modified. ${t} anyway?`)||(s.Util.clearTerminal(),""!==e&&(e=e.trim()+"\n"),u.setValue(e,-1),u.session.getUndoManager().reset(),u.gotoLine(0,0),u.focus(),0))}const y=(()=>{const e=l.getEditor(1);return e.$blockScrolling=1/0,e.setOptions({tabSize:2,useSoftTabs:!0,printMarginColumn:!1}),e.setReadOnly(!0),e.getSession().setMode("ace/mode/text"),e.setValue("XCC browser version.\n",-1),e})();function p(e){return encodeURIComponent(e).replace(/[!'()*]/g,(e=>"%"+e.charCodeAt(0).toString(16)))}function f(){""!==window.location.search&&window.history.replaceState(null,document.title,window.location.pathname)}s.Util.setTerminal(y),window.addEventListener("load",(()=>{const e=new o.WaStorage;let t;async function n(){if(null==t)return;let n;s.Util.clearTerminal(),u.focus();try{if(n=await async function(n){const r="main.c",s=new i.WaProc(e);s.chdir("/home/wasm"),s.saveFile(r,n);const o=["cc","-I/usr/include","-emain",r,"/usr/lib/lib.c"];return 0!==await s.runWasmMain(t,o)?null:s.loadFile("a.wasm")}(u.getValue()),null==n)return}catch(e){return void(e instanceof i.ExitCalledError||s.Util.putTerminalError(e))}const o=new i.WaProc(e);o.chdir("/home/wasm"),o.registerCFunction("showGraphic",((e,t,n)=>{const s=r.DomUtil.createCanvas(e,t);r.DomUtil.setStyles(s,{display:"block"}),function(e,t,n){const r=e.getContext("2d"),s=r.getImageData(0,0,e.width,e.height);s.data.set(new Uint8Array(t.buffer,n,e.width*e.height*4)),r.putImageData(s,0,0)}(s,o.getLinearMemory(),n);const i=document.createElement("div");i.className="wnd draggable",r.DomUtil.setStyles(i,{zIndex:1e4}),i.appendChild(s),r.DomUtil.makeDraggable(i),r.DomUtil.addDivButton(i,(()=>{var e;null===(e=i.parentNode)||void 0===e||e.removeChild(i)})).className="close-button",document.body.appendChild(i),r.DomUtil.putOnCenter(i)}));const a=document.getElementById("args").value.trim(),c=""===a?[]:a.trim().split(/\s+/);c.unshift("a.wasm");try{const e=await o.runWasmMain(n,c);0!=e&&console.error(`Exit code=${e}`)}catch(e){return void(e instanceof i.ExitCalledError||s.Util.putTerminalError(e))}}function d(){(function(){const e=u.getValue();localStorage.setItem(c,e),u.session.getUndoManager().markClean()})(),alert("Saved!"),window.location.hash=""}s.Util.loadFromServer("libs.json").then((t=>{!function t(n,r){for(const s of Object.keys(r)){const i=`${n}/${s}`;"string"==typeof r[s]?e.putFile(i,r[s]):t(i,r[s])}}("",JSON.parse(t))})),s.Util.loadFromServer("cc.wasm",{binary:!0}).then((e=>t=new Uint8Array(e))),document.getElementById("run").addEventListener("click",n),u.commands.addCommands([{Name:"Run",bindKey:{win:"Ctrl-Enter",mac:"Command-Enter"},exec:e=>n()},{name:"Save",bindKey:{win:"Ctrl-S",mac:"Command-S",sender:"editor|cli"},exec:(e,t,n)=>d()}]);const y=new URLSearchParams(window.location.search);y.has("code")?(m(y.get("code")||"",""),document.getElementById("args").value=y.get("args")||""):function(){const e=localStorage.getItem(c);return null!=e&&(m(e,""),!0)}()||m(a.ExampleCodes.hello,"Hello"),window.addEventListener("resize",(()=>{l.resize()}),!1);const g=document.getElementById("sysmenu"),b=document.getElementById("share");function v(){r.DomUtil.setStyles(g,{display:"none"})}document.getElementById("nav-open").addEventListener("click",(()=>{const e=u.getValue().trim(),t=document.getElementById("share-section");if(""!==e){const n=document.getElementById("args").value.trim();r.DomUtil.setStyles(t,{display:null}),b.href=`?code=${p(e)}&args=${p(n)}`,delete b.dataset.disabled}else r.DomUtil.setStyles(t,{display:"none"}),b.dataset.disabled="disabled";r.DomUtil.setStyles(g,{display:null})})),g.addEventListener("click",v);const w=document.getElementById("example-select");w.addEventListener("change",(e=>{const t=w.value;w.value="",v(),f(),m(a.ExampleCodes[t],`Load "${[].slice.call(w.options).find((e=>e.value===t)).text}"`),document.getElementById("args").value=""})),document.getElementById("new").addEventListener("click",(e=>(e.preventDefault(),v(),m("","New"),f(),!1))),document.getElementById("save").addEventListener("click",(e=>(e.preventDefault(),v(),d(),f(),!1))),b.addEventListener("click",(e=>{if(e.preventDefault(),"disabled"!==b.dataset.disabled){v();const e=new URL(b.href);let t=e.pathname;e.search&&(t+=e.search),window.history.replaceState(null,document.title,t),navigator.clipboard.writeText(b.href).then((e=>alert("URL copied!")))}return!1})),window.addEventListener("beforeunload",(e=>{h()&&(e.preventDefault(),e.returnValue="")}))}))},507:(e,t)=>{let n;Object.defineProperty(t,"__esModule",{value:!0}),t.Util=void 0;class r{static clamp(e,t,n){return n<t||e<t?t:e>n?n:e}static decodeString(e,t,n){const r=new Uint8Array(e,t,n);let s;for(s=0;s<r.length&&0!==r[s];++s);const i=new Uint8Array(e,t,s);return new TextDecoder("utf-8").decode(i)}static async loadFromServer(e,t=null){const n=await fetch(e,{method:"GET"});return null!=t&&t.binary?await n.arrayBuffer():await n.text()}static encode(e){return(new TextEncoder).encode(e)}static setTerminal(e){n=e}static putTerminal(e){n.session.insert(n.getCursorPosition(),e.toString())}static putTerminalError(e){console.error(e),r.putTerminal(e)}static clearTerminal(){n.setValue("",-1)}}t.Util=r},695:(e,t,n)=>{Object.defineProperty(t,"__esModule",{value:!0}),t.WaProc=t.ExitCalledError=void 0;const r=n(949),s=n(507);function i(e,t){return e+t-1&-t}class o extends Error{constructor(e){super(`Exit code: ${e}`),this.code=e}}t.ExitCalledError=o,t.WaProc=class{constructor(e){this.breakStartAddress=0,this.breakAddress=0,this.cwd="/",this.fs=new r.FileSystem(e),this.memory=new WebAssembly.Memory({initial:10,maximum:100}),this.imports=this.createImports()}brk(e){if(e>=this.breakStartAddress){if(e>this.memory.buffer.byteLength){const t=e-this.memory.buffer.byteLength,n=Math.floor((t+65536-1)/65536);try{this.memory.grow(n)}catch(e){return console.error(e),this.breakAddress}}this.breakAddress=e}return this.breakAddress}getAbsPath(e){return e.length>0&&"/"===e[0]?e:`${this.cwd}${"/"===this.cwd?"":"/"}${e}`}chdir(e){this.cwd=e}saveFile(e,t){this.fs.saveFile(this.getAbsPath(e),t)}loadFile(e){return this.fs.loadFile(this.getAbsPath(e))}async runWasmMain(e,t){const n=await this.loadWasm(e),r=this.putArgs(t);return n.exports.main(t.length,r)}async loadWasm(e){let t;if("string"==typeof e)if(WebAssembly.instantiateStreaming)t=await WebAssembly.instantiateStreaming(fetch(e),this.imports);else{const n=await fetch(e),r=await n.arrayBuffer();t=await WebAssembly.instantiate(r,this.imports)}else{if(e.constructor!==Uint8Array)return console.error(`Path or buffer required: ${e}`),null;t=await WebAssembly.instantiate(e,this.imports)}const n=t.instance,r=n.exports.$_SP;return null!=r&&(this.breakStartAddress=this.breakAddress=i(r.valueOf(),8)),n}registerCFunction(e,t){this.imports.c[e]=t}getLinearMemory(){return this.memory}putArgs(e){const t=e.map(s.Util.encode),n=t.reduce(((e,t)=>e+t.length+1),0),r=4*(e.length+1)+n,o=this.breakAddress,a=o+4*(e.length+1);this.brk(i(o+r,8));const c=new Uint32Array(this.memory.buffer,o,e.length+1),l=new Uint8Array(this.memory.buffer,a,n);let d=0;for(let n=0;n<e.length;++n){const e=t[n],r=e.length;for(let t=0;t<r;++t)l[d+t]=e[t];l[d+r]=0,c[n]=a+d,d+=r+1}return c[e.length]=0,o}createImports(){return{c:{read:(e,t,n)=>{const r=new Uint8Array(this.memory.buffer,t,n);return this.fs.read(e,r)},write:(e,t,n)=>{const r=new Uint8Array(this.memory.buffer,t,n);return this.fs.write(e,r)},open:(e,t,n)=>{if(0===e)return-1;const r=s.Util.decodeString(this.memory.buffer,e);if(null==r||""===r)return-1;const i=this.getAbsPath(r);return this.fs.open(i,t,n)},close:e=>this.fs.close(e),lseek:(e,t,n)=>this.fs.lseek(e,t,n),_tmpfile:()=>this.fs.tmpfile(),_brk:e=>this.brk(e),_getcwd:(e,t)=>{const n=s.Util.encode(this.cwd),r=n.length;if(r+1>t)return-34;const i=new Uint8Array(this.memory.buffer,e,r+1);for(let e=0;e<r;++e)i[e]=n[e];return i[r]=0,r+1},sin:Math.sin,cos:Math.cos,sqrt:Math.sqrt,drand48:Math.random,fabs:Math.abs,putstr:e=>{const t=s.Util.decodeString(this.memory.buffer,e);s.Util.putTerminal(t)},puti:e=>{s.Util.putTerminal(e)},exit:e=>{throw new o(e)},_memcpy:(e,t,n)=>{new Uint8Array(this.memory.buffer).copyWithin(e,t,t+n)}},env:{memory:this.memory}}}}}},t={};!function n(r){if(t[r])return t[r].exports;var s=t[r]={exports:{}};return e[r](s,s.exports,n),s.exports}(697)})();