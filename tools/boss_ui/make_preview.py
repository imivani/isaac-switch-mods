"""Create a self-contained canvas preview from the unchanged upstream art.

This is a layout/reference preview, not a screenshot or native-engine test.
"""
import base64
import json
from pathlib import Path
import posixpath
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[2]
OUT = PROJECT / "evidence/boss_ui"
ASSETS = PROJECT / "assets/boss_ui/resources"


def main():
    model = json.loads((OUT / "model.json").read_text())
    animations = []
    for definition in model["animations"]:
        root = ET.parse(ASSETS / definition[0]).getroot()
        sheets = {int(sheet.attrib["Id"]): posixpath.normpath(posixpath.join(posixpath.dirname(definition[0]), sheet.attrib["Path"].replace("\\", "/")))
                  for sheet in root.findall("Content/Spritesheets/Spritesheet")}
        layers = {int(layer.attrib["Id"]): sheets[int(layer.attrib["SpritesheetId"])] for layer in root.findall("Content/Layers/Layer")}
        sequences = {}
        for sequence in root.findall("Animations/Animation"):
            sequences[sequence.attrib["Name"]] = {
                "length": int(sequence.attrib["FrameNum"]),
                "root": [frame.attrib for frame in sequence.findall("RootAnimation/Frame")],
                "layers": [{"visible": layer.attrib.get("Visible", "true") == "true", "image": layers[int(layer.attrib["LayerId"])],
                            "frames": [frame.attrib for frame in layer.findall("Frame")]} for layer in sequence.findall("LayerAnimations/LayerAnimation")]
            }
        animations.append(sequences)
    images = {path.relative_to(ASSETS).as_posix(): "data:image/png;base64," + base64.b64encode(path.read_bytes()).decode()
              for path in ASSETS.rglob("*.png")}
    model["animationFrames"] = animations
    model["images"] = images
    template = r'''<!doctype html><html lang="en"><meta charset="utf-8"><title>Enhanced Boss Bars — source-art preview</title>
<meta name="viewport" content="width=device-width, initial-scale=1"><style>
*{box-sizing:border-box}body{margin:0;padding:30px;background:#151619;color:#e4e4e6;font:14px system-ui,sans-serif}main{max-width:1010px;margin:auto}h1{font-size:25px;margin:0 0 8px}p{color:#a9aab1;line-height:1.6;margin:4px 0 18px}.controls{display:flex;gap:14px;align-items:center;margin:22px 0}input{width:200px;accent-color:#bb352f}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}article{border:1px solid #35373b;border-radius:8px;overflow:hidden;background:#1e2024}h2{font-size:14px;font-weight:500;margin:13px 17px}canvas{display:block;width:100%;image-rendering:pixelated;background:#3d342f}.note{font-size:12px;margin-top:22px} @media(max-width:700px){.grid{grid-template-columns:1fr}body{padding:20px}}
</style><main><h1>Enhanced Boss Bars</h1><p>The original portraits, textured bars, and compact spacing.<br>This is a source-art layout preview, not a Switch screenshot.</p>
<div class="controls"><label for="hp">Boss health</label><input id="hp" type="range" min="1" max="100" value="64"><output id="percent">64%</output></div>
<div class="grid"><article><h2>One boss · original red bar</h2><canvas id="single" width="480" height="135"></canvas></article><article><h2>Two bosses · paired bars</h2><canvas id="double" width="480" height="135"></canvas></article><article><h2>Special boss artwork</h2><canvas id="special" width="480" height="135"></canvas></article><article><h2>Seven bosses · compact row</h2><canvas id="many" width="480" height="135"></canvas></article></div>
<p class="note">Default PC layout: boss portraits instead of test labels; bars stay at the bottom. Mother has her original overlay and Dogma uses its animated artwork. Gideon keeps the game’s wave counter. Champion recoloring, status icons, and some body-segment rules remain outside this visual pass.</p></main>
<script>const model=__MODEL__; const images={}; let tick=0;
function pick(frames,index){let frame=null;for(const f of frames){frame=f;index-=Number(f.Delay??1);if(index<0)break;}return frame;}
function number(frame,key,fallback){return Number(frame?.[key]??fallback)}
function paint(context,asset,sheet,name,index,x,y,crop=0,color=[1,1,1,1]){
 const animation=model.animationFrames[asset][name];if(!animation)return;
 const root=pick(animation.root,index), rx=number(root,'XScale',100)/100, ry=number(root,'YScale',100)/100;
 for(const layer of animation.layers){if(!layer.visible)continue;const f=pick(layer.frames,index);if(!f||f.Visible==='false')continue;
  const image=images[sheet||layer.image],w=number(f,'Width',0),h=number(f,'Height',0);if(!image||!w||!h)continue;
  const bitmap=document.createElement('canvas');bitmap.width=w;bitmap.height=h;const temp=bitmap.getContext('2d');temp.imageSmoothingEnabled=false;
  temp.drawImage(image,number(f,'XCrop',0),number(f,'YCrop',0),w,h,0,0,w,h);
  const pixels=temp.getImageData(0,0,w,h);const tint=['RedTint','GreenTint','BlueTint','AlphaTint'].map((k,j)=>number(f,k,255)/255*number(root,k,255)/255*color[j]);
  for(let i=0;i<pixels.data.length;i+=4)for(let j=0;j<4;j++)pixels.data[i+j]*=tint[j];temp.putImageData(pixels,0,0);
  context.save();context.translate(x+number(root,'XPosition',0)+number(f,'XPosition',0),y+number(root,'YPosition',0)+number(f,'YPosition',0));
  context.rotate((number(root,'Rotation',0)+number(f,'Rotation',0))*Math.PI/180);context.scale(rx*number(f,'XScale',100)/100,ry*number(f,'YScale',100,100)/100);
  const visible=Math.max(0,w-crop);if(visible>0)context.drawImage(bitmap,0,0,visible,h,-number(f,'XPivot',0),-number(f,'YPivot',0),visible,h);context.restore();
 }
}
function sequenceFrame(asset,name,animated,hp){const length=model.animationFrames[asset][name].length;return animated?tick%length:Math.min(hp,length-1)}
function draw(canvas,bosses,hp){const ctx=canvas.getContext('2d');ctx.imageSmoothingEnabled=false;ctx.clearRect(0,0,canvas.width,canvas.height);
 const size=bosses.length<=2?120:bosses.length<=4?60:30;const suffix=size===120?'100':size===60?'50':'25';
 const start=240-bosses.length*size/2-(bosses.length-1)*15;const hpFrame=100-Math.min(Math.ceil(hp),99);const lost=size/100*hpFrame;
 for(let i=0;i<bosses.length;i++){const [type,variant]=bosses[i],boss=model.bosses.find(b=>b[0]===type&&b[1]===variant),style=model.styles[boss[4]];
  const x=start+i*(size+30),y=canvas.height-23;
  paint(ctx,style[2],style[1],'bg'+suffix,sequenceFrame(style[2],'bg'+suffix,style[3],hpFrame),x,y);
  paint(ctx,style[2],style[1],'charge'+suffix,sequenceFrame(style[2],'charge'+suffix,style[3],hpFrame),x,y,1+lost,style[7]);
  paint(ctx,style[2],style[1],'endDeco',sequenceFrame(style[2],'endDeco',style[3],hpFrame),x+size-1-lost,y,0,style[7]);
  if(style[4]>=0)paint(ctx,style[4],style[5],'overlay'+suffix,sequenceFrame(style[4],'overlay'+suffix,style[6],hpFrame),x,y);
  paint(ctx,boss[3],boss[2],'idle',sequenceFrame(boss[3],'idle',boss[5],hpFrame),x+boss[6]-3,y+boss[7]);
 }
}
function render(){const hp=Number(document.querySelector('#hp').value);document.querySelector('#percent').value=hp+'%';
draw(document.querySelector('#single'),[[20,0]],hp);draw(document.querySelector('#double'),[[20,0],[36,0]],hp);
draw(document.querySelector('#special'),[[407,0],[912,10],[950,1],[275,0]],hp);
draw(document.querySelector('#many'),[[20,0],[36,0],[67,0],[79,0],[69,0],[99,0],[100,0]],hp);}
Promise.all(Object.entries(model.images).map(([path,url])=>new Promise(resolve=>{const image=new Image;image.onload=()=>{images[path]=image;resolve()};image.src=url}))).then(()=>{render();setInterval(()=>{tick++;render()},1000/30)});
</script></html>'''
    html = template.replace("__MODEL__", json.dumps(model, separators=(",", ":")))
    path = OUT / "preview.html"
    path.write_text(html, encoding="utf-8")
    print(path)


if __name__ == "__main__":
    main()
