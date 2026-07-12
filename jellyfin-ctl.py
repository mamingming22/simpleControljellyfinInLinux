import tkinter as tk, subprocess as sp
def c(v):
    sp.run(["pkexec","systemctl",v,"jellyfin"]); s()
def s():
    a=sp.run(["systemctl","is-active","jellyfin"],capture_output=True,text=True).stdout.strip()=="active"
    b.config(state="disabled" if a else "normal"); p.config(state="normal" if a else "disabled"); t.config(text=f"{'运行中' if a else '已停止'}")
r=tk.Tk(); r.title("Jellyfin"); r.geometry("260x100")
t=tk.Label(r,font=("",16)); t.pack(pady=15)
f=tk.Frame(r); f.pack()
b=tk.Button(f,text="启动",command=lambda:c("start")); b.pack(side="left",padx=8)
p=tk.Button(f,text="停止",command=lambda:c("stop")); p.pack(side="right",padx=8)
s(); r.mainloop()
