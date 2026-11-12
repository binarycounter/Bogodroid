import json
import pprint
import sys

fp = open(sys.argv[1])
data=json.load(fp)


print("Loaded",len(data),"entries")


methods=set()
cmethods=set()
csmethods=set()
cfields=set()
csfields=set()
classes=set()

for entry in data:
    entry.pop('timestamp', None)
    entry.pop('thread_id', None)
    methods.add(entry["method"]["name"])
    if entry["method"]["name"]=="FindClass":
        classes.add(entry["ret"]["metadata"])
    if entry["method"]["name"]=="GetMethodID" or entry["method"]["name"]=="GetStaticMethodID" or entry["method"]["name"]=="GetStaticFieldID" or entry["method"]["name"]=="GetFieldID":
        if "metadata" not in entry["args"][1]:
            entry["args"][1]["metadata"]=str(entry["args"][1]["value"])
        if "data" not in entry["args"][2]:
            entry["args"][2]["data"]=str(entry["args"][2]["value"])
        if "data" not in entry["args"][3]:
            entry["args"][3]["data"]=str(entry["args"][3]["value"])
        if entry["method"]["name"]=="GetMethodID":
            cmethods.add((entry["args"][1]["metadata"],entry["args"][2]["data"],entry["args"][3]["data"]))
        if entry["method"]["name"]=="GetStaticMethodID":
            csmethods.add((entry["args"][1]["metadata"],entry["args"][2]["data"],entry["args"][3]["data"]))
        if entry["method"]["name"]=="GetFieldID":
            cfields.add((entry["args"][1]["metadata"],entry["args"][2]["data"],entry["args"][3]["data"]))
        if entry["method"]["name"]=="GetStaticFieldID":
            csfields.add((entry["args"][1]["metadata"],entry["args"][2]["data"],entry["args"][3]["data"]))
    
    
print("--------- JNIENV->METHODS ----------")
print(methods)
print("--------- CLASSES ----------")
print(classes)
print("--------- CLASS->METHODS ----------")
print(cmethods)
print("--------- CLASS->METHODS STATIC ----------")
print(csmethods)
print("--------- CLASS->FIELDS ----------")
print(cfields)
print("--------- CLASS->FIELDS STATIC ----------")
print(csfields)




classtree=dict()

for m in classes:
    if m not in classtree:
        classtree[m]=dict()
        classtree[m]["member"]=list()
        classtree[m]["static"]=list()
        classtree[m]["fields"]=list()
        classtree[m]["fields_static"]=list()

for m in cmethods:
    if m[0] not in classtree:
        classtree[m[0]]=dict()
        classtree[m[0]]["member"]=list()
        classtree[m[0]]["static"]=list()
        classtree[m[0]]["fields"]=list()
        classtree[m[0]]["fields_static"]=list()
    classtree[m[0]]["member"].append((m[1],m[2]))

for m in csmethods:
    if m[0] not in classtree:
        classtree[m[0]]=dict()
        classtree[m[0]]["member"]=list()
        classtree[m[0]]["static"]=list()
        classtree[m[0]]["fields"]=list()
        classtree[m[0]]["fields_static"]=list()
    classtree[m[0]]["static"].append((m[1],m[2]))

for m in cfields:
    if m[0] not in classtree:
        classtree[m[0]]=dict()
        classtree[m[0]]["member"]=list()
        classtree[m[0]]["static"]=list()
        classtree[m[0]]["fields"]=list()
        classtree[m[0]]["fields_static"]=list()
    classtree[m[0]]["fields"].append((m[1],m[2]))

for m in csfields:
    if m[0] not in classtree:
        classtree[m[0]]=dict()
        classtree[m[0]]["member"]=list()
        classtree[m[0]]["static"]=list()
        classtree[m[0]]["fields"]=list()
        classtree[m[0]]["fields_static"]=list()
    classtree[m[0]]["fields_static"].append((m[1],m[2]))



for c in classtree.values():
    if len(c["member"])==0:
        del c["member"]
    if len(c["static"])==0:
        del c["static"] 
    if len(c["fields"])==0:
        del c["fields"] 
    if len(c["fields_static"])==0:
        del c["fields_static"] 
    for t in c:
        c[t].sort(key=lambda x: x[0])
    

#pprint.pprint(classtree)
classtree=dict(sorted(classtree.items()))

try:
    import jsbeautifier
    treestring=jsbeautifier.beautify(json.dumps(classtree))
except ModuleNotFoundError:
    print("jsbeautifier not installed")
    treestring=json.dumps(classtree,indent=8)



#print(treestring)
open(open(sys.argv[2]),"w").write(treestring)

