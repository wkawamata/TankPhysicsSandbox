#include "Map/MapSelection.h"
#include "App/ManifestUndoEdit.h"
#include <DirectXMath.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool value,const char* message) {if (!value) throw std::runtime_error(message);}
    bool Close(float a,float b) {return std::abs(a-b)<0.001f;}
    bool SamePosition(const std::array<float,3>& a,const std::array<float,3>& b)
    {return Close(a[0],b[0]) && Close(a[1],b[1]) && Close(a[2],b[2]);}
    std::array<float,16> Matrix(DirectX::FXMMATRIX value)
    {
        DirectX::XMFLOAT4X4 stored; DirectX::XMStoreFloat4x4(&stored,value);
        std::array<float,16> data; std::copy(&stored._11,&stored._11+16,data.begin()); return data;
    }
}
int main()
{
    using namespace Tank::Map;
    using namespace DirectX;
    try
    {
        Check(SelectNode({"mesh/a"},"mesh/b",true)==std::vector<std::string>({"mesh/a","mesh/b"}), "Shift adds to selection and makes the final entry active");
        Check(SelectNode({"mesh/a","mesh/b"},"mesh/b",true)==std::vector<std::string>({"mesh/a"}), "Shift toggles an existing element off");
        Check(SelectNode({"mesh/a","mesh/b"},"mesh/c",false)==std::vector<std::string>({"mesh/c"}), "plain click replaces selection");
        Check(SelectNode({"root"},"mesh/a",true)==std::vector<std::string>({"mesh/a"}), "Root is not accidentally included in a group");
        Manifest source;
        source.instances = {{"a","a.gltf",{{2,0,0},{},{2,1,1}}}, {"b","a.gltf",{{8,0,0},{0,45,0}}}};
        const std::vector<std::string> ids = {"mesh/a","mesh/b"};
        const auto reference = *NodeWorldTransform(source,"mesh/a").worldMatrix;
        auto moved = source;
        auto after = reference; after[12]+=3; after[13]+=2;
        std::string error;
        Check(TransformSelection(moved,ids,reference,after,SelectionTransform::Translate,false,error), "group translates");
        Check(Close(NodeWorldTransform(moved,"mesh/a").position[0],5) && Close(NodeWorldTransform(moved,"mesh/b").position[0],11), "same world delta is applied to all nodes");
        auto rotated = source;
        const auto rotation = Matrix(XMMatrixScaling(2,1,1)*XMMatrixRotationY(XM_PIDIV2)*XMMatrixTranslation(2,0,0));
        Check(TransformSelection(rotated,ids,reference,rotation,SelectionTransform::Rotate,false,error), "group rotates");
        for (const auto& id : ids)
            Check(SamePosition(NodeWorldTransform(rotated,id).position,NodeWorldTransform(source,id).position),"rotation preserves each individual origin");
        Check(Close(NodeWorldTransform(rotated,"mesh/b").rotationDegrees[1],135),"rotation adds the same world angle");
        Check(Close(NodeWorldTransform(rotated,"mesh/a").scale[0],2),"rotation preserves nonuniform object size");
        auto scaled = source;
        const auto scale = Matrix(XMMatrixScaling(4,3,2)*XMMatrixTranslation(2,0,0));
        Check(TransformSelection(scaled,ids,reference,scale,SelectionTransform::Scale,true,error),"group scales");
        for (const auto& id : ids)
        {
            const auto before = NodeWorldTransform(source,id), result = NodeWorldTransform(scaled,id);
            Check(SamePosition(before.position,result.position),"scale preserves each individual origin");
            Check(Close(result.scale[0],before.scale[0]*2) && Close(result.scale[1],before.scale[1]*3),"scale multiplies each individual size");
        }
        auto hierarchy = source;
        hierarchy.instances[0].transform.scale={1,1,1};
        hierarchy.instances[1].parentId="mesh/a";
        auto transformed = hierarchy;
        Check(TransformSelection(transformed,{"mesh/b","mesh/a"},reference,after,SelectionTransform::Translate,false,error),"selected parents are applied before selected children");
        Check(Close(NodeWorldTransform(transformed,"mesh/b").position[0],NodeWorldTransform(hierarchy,"mesh/b").position[0]+3),"selected child is translated once, not twice");
        Check(TransformSelection(transformed=hierarchy,{"mesh/b","mesh/a"},reference,rotation,SelectionTransform::Rotate,false,error),"selected parent and child rotate independently");
        Check(SamePosition(NodeWorldTransform(transformed,"mesh/b").position,NodeWorldTransform(hierarchy,"mesh/b").position),"selected child keeps its own origin while its parent rotates");
        std::vector<std::string> copies;
        auto duplicated = hierarchy;
        Check(DuplicateSelection(duplicated,{"mesh/a","mesh/b"},copies,error) && copies.size()==1 && duplicated.instances.size()==4,"selected subtree is duplicated once");
        auto siblings = source;
        Check(DuplicateSelection(siblings,ids,copies,error) && copies.size()==2 && siblings.instances.size()==4,"separate selected elements are duplicated together");
        auto removed = hierarchy;
        Check(!RemoveSelection(removed,{"mesh/a"},error) && Tank::App::ManifestUndoEqual{}(removed,hierarchy),"deletion with an unselected child is atomic");
        Check(RemoveSelection(removed,{"mesh/a","mesh/b"},error) && removed.instances.empty(),"selected children are deleted before their parent");
        Check(!RemoveSelection(removed,{"player-start"},error),"the last start cannot be removed");
        auto starts=source; starts.startPoints={{"a",{}},{"b",{}}};
        Check(RemoveSelection(starts,{"player-start","start/a"},error) && starts.startPoints.empty(),"multi-delete promotes an unselected start without deleting the wrong element");
        auto rigid=source; rigid.enemies.push_back({"enemy","type"});
        Check(!CanScaleSelection(rigid,{"mesh/a","enemy/enemy"}),"fixed-size markers keep the existing scale restriction");
        Check(!TransformSelection(rigid,{"mesh/a","enemy/enemy"},reference,scale,SelectionTransform::Scale,true,error) &&
            Close(rigid.instances[0].transform.scale[0],2),"unsupported mixed scaling makes no partial changes");
        Tank::App::UndoHistory history;
        Tank::App::ManifestUndoEdit edit;
        auto document=source;
        const auto apply=[&](const auto& value) {document=value;};
        auto before=document;
        Check(TransformSelection(document,ids,reference,after,SelectionTransform::Translate,false,error),"first drag sample");
        edit.Update(before,document,7,history,apply,"Group transform");
        before=document;
        auto next=after; next[12]+=1;
        Check(TransformSelection(document,ids,after,next,SelectionTransform::Translate,false,error),"second drag sample");
        edit.Update(before,document,7,history,apply,"Group transform"); edit.Flush(history,apply,"Group transform");
        Check(history.UndoCount()==1,"continuous group drag is one Undo command");
        const auto final=document; history.Undo();
        Check(Tank::App::ManifestUndoEqual{}(document,source),"Undo restores all selected elements");
        history.Redo(); Check(Tank::App::ManifestUndoEqual{}(document,final),"Redo restores the complete group edit");
        std::cout<<"PASS MapSelection\n"; return 0;
    }
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
