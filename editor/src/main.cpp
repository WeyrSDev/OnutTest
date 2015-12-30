// Required for WinMain
#include <Windows.h>

// Oak Nut include
#include "seed/View.h"
#include "seed/Sprite.h"
#include "onut.h"
#include "ActionManager.h"
#include "menu.h"

#include <unordered_map>

void createUIStyles(onut::UIContext* pContext);
void init();
void update();
void render();

enum class State
{
    Idle,
    Panning,
    Zooming,
    Moving,
    MovingHandle,
    Rotate,
    IsAboutToRotate,
    IsAboutToMove,
    IsAboutToMoveHandle,
};

class NodeContainer;

struct SpriteState
{
    std::string texture;
    Vector2 position;
    Vector2 scale;
    float angle;
    Color color;
    Vector2 align;
    Matrix transform;
    Matrix parentTransform;
    NodeContainer* pContainer = nullptr;

    SpriteState() {}
    SpriteState(const SpriteState& copy);
    SpriteState(NodeContainer* in_pContainer);
    void apply() const;
};

class NodeContainer : public onut::Object
{
public:
    NodeContainer() { retain(); }
    seed::Node* pNode = nullptr;
    onut::UITreeViewItem* pTreeViewItem = nullptr;
    SpriteState stateOnDown;
};

SpriteState::SpriteState(const SpriteState& copy)
{
    texture = copy.texture;
    position = copy.position;
    scale = copy.scale;
    angle = copy.angle;
    color = copy.color;
    align = copy.align;
    transform = copy.transform;
    parentTransform = copy.parentTransform;
    pContainer = copy.pContainer;
}

SpriteState::SpriteState(NodeContainer* in_pContainer)
{
    pContainer = in_pContainer;
    assert(pContainer);
    auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
    if (pSprite)
    {
        texture = pSprite->GetTexture()->getName();
        align = pSprite->GetAlign();
    }
    position = pContainer->pNode->GetPosition();
    scale = pContainer->pNode->GetScale();
    angle = pContainer->pNode->GetAngle();
    color = pContainer->pNode->GetColor();
    transform = pContainer->pNode->GetTransform();
    parentTransform = Matrix::Identity;
    if (pContainer->pNode->GetParent())
    {
        parentTransform = pContainer->pNode->GetParent()->GetTransform();
    }
}

void SpriteState::apply() const
{
    assert(pContainer);
    auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
    if (pSprite)
    {
        pSprite->SetTexture(OGetTexture(texture.c_str()));
        pSprite->SetAlign(align);
    }
    pContainer->pNode->SetPosition(position);
    pContainer->pNode->SetScale(scale);
    pContainer->pNode->SetAngle(angle);
    pContainer->pNode->SetColor(color);
}

enum class Handle
{
    TOP_LEFT,
    LEFT,
    BOTTOM_LEFT,
    BOTTOM,
    BOTTOM_RIGHT,
    RIGHT,
    TOP_RIGHT,
    TOP
};

struct TransformHandle
{
    Vector2 screenPos;
    Handle handle = Handle::TOP_LEFT;
    Vector2 transformDirection;
};

using TransformHandles = std::vector<TransformHandle>;
using AABB = std::vector<Vector2>;
using HandleIndex = AABB::size_type;

struct Gizmo
{
    TransformHandles transformHandles;
    AABB aabb;
};

// Utilities
onut::ActionManager actionManager;
std::unordered_map<seed::Node*, NodeContainer*> nodesToContainers;

// Camera
using Zoom = float;
using ZoomIndex = int;
static const std::vector<Zoom> zoomLevels = {.20f, .50f, .70f, 1.f, 1.5f, 2.f, 4.f};
ZoomIndex zoomIndex = 3;
Zoom zoom = zoomLevels[zoomIndex];
Vector2 cameraPos = Vector2::Zero;

// Selection
using Selection = std::vector<NodeContainer*>;
Selection selection;
OAnimf dottedLineAnim = 0.f;
Gizmo gizmo;

// State
State state = State::Idle;
Vector2 cameraPosOnDown;
onut::sUIVector2 mousePosOnDown;
Vector2 selectionCenter;
HandleIndex handleIndexOnDown;

// Controls
onut::UIControl* pMainView = nullptr;
onut::UITreeView* pTreeView = nullptr;

onut::UITextBox* pPropertyName = nullptr;
onut::UITextBox* pPropertyClass = nullptr;
onut::UITextBox* pPropertyTexture = nullptr;
onut::UIButton* pPropertyTextureBrowse = nullptr;
onut::UITextBox* pPropertyX = nullptr;
onut::UITextBox* pPropertyY = nullptr;
onut::UITextBox* pPropertyScaleX = nullptr;
onut::UITextBox* pPropertyScaleY = nullptr;
onut::UITextBox* pPropertyAlignX = nullptr;
onut::UITextBox* pPropertyAlignY = nullptr;
onut::UITextBox* pPropertyAngle = nullptr;
onut::UIPanel* pPropertyColor = nullptr;
onut::UITextBox* pPropertyAlpha = nullptr;

// Seed
seed::View* pEditingView = nullptr;
Vector2 viewSize = Vector2(640, 480);

void updateTransformHandles();

void updateProperties()
{
    pPropertyName->isEnabled = false;
    pPropertyClass->isEnabled = false;
    pPropertyTexture->isEnabled = false;
    pPropertyTextureBrowse->isEnabled = false;
    pPropertyX->isEnabled = false;
    pPropertyY->isEnabled = false;
    pPropertyScaleX->isEnabled = false;
    pPropertyScaleY->isEnabled = false;
    pPropertyAlignX->isEnabled = false;
    pPropertyAlignY->isEnabled = false;
    pPropertyAngle->isEnabled = false;
    pPropertyColor->isEnabled = false;
    pPropertyAlpha->isEnabled = false;

    if (!selection.empty())
    {
        pPropertyTexture->isEnabled = true;
        pPropertyTextureBrowse->isEnabled = true;
        pPropertyAlignX->isEnabled = true;
        pPropertyAlignY->isEnabled = true;
    }

    for (auto pContainer : selection)
    {
        pPropertyName->isEnabled = true;
        pPropertyClass->isEnabled = true;
        pPropertyX->isEnabled = true;
        pPropertyY->isEnabled = true;
        pPropertyScaleX->isEnabled = true;
        pPropertyScaleY->isEnabled = true;
        pPropertyAngle->isEnabled = true;
        pPropertyColor->isEnabled = true;
        pPropertyAlpha->isEnabled = true;

        pPropertyName->textComponent.text = "";
        pPropertyClass->textComponent.text = "";

        auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
        if (pSprite)
        {
            auto pTexture = pSprite->GetTexture();
            if (pTexture)
            {
                pPropertyTexture->textComponent.text = pTexture->getName();
            }
            else
            {
                pPropertyTexture->textComponent.text = "";
            }
            pPropertyAlignX->setFloat(pSprite->GetAlign().x);
            pPropertyAlignY->setFloat(pSprite->GetAlign().y);
        }
        else
        {
            pPropertyTexture->isEnabled = false;
            pPropertyTextureBrowse->isEnabled = false;
            pPropertyAlignX->isEnabled = false;
            pPropertyAlignY->isEnabled = false;
        }
        pPropertyX->setFloat(pContainer->pNode->GetPosition().x);
        pPropertyY->setFloat(pContainer->pNode->GetPosition().y);
        pPropertyScaleX->setFloat(pContainer->pNode->GetScale().x);
        pPropertyScaleY->setFloat(pContainer->pNode->GetScale().y);
        pPropertyAngle->setFloat(pContainer->pNode->GetAngle());
        pPropertyColor->color = onut::sUIColor(pContainer->pNode->GetColor().x, pContainer->pNode->GetColor().y, pContainer->pNode->GetColor().z, pContainer->pNode->GetColor().w);
        pPropertyAlpha->setFloat(pContainer->pNode->GetColor().w * 100.f);
    }

    updateTransformHandles();
}

// Main
int CALLBACK WinMain(HINSTANCE appInstance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdCount)
{
    // Set default settings
    OSettings->setBorderlessFullscreen(false);
    OSettings->setGameName("Seed Editor");
    OSettings->setIsResizableWindow(false);
    OSettings->setResolution({1280, 720});

    // Run
    ORun(init, update, render);
}

Matrix getViewTransform()
{
    auto viewRect = pMainView->getWorldRect(*OUIContext);
    Matrix transform =
        Matrix::CreateTranslation(-cameraPos) *
        Matrix::CreateScale(zoom) *
        Matrix::CreateTranslation(Vector2((float)viewRect.position.x + (float)viewRect.size.x * .5f, (float)viewRect.position.y + (float)viewRect.size.y * .5f));
    return std::move(transform);
}

std::vector<Vector2> getSpriteCorners(seed::Sprite* pSprite)
{
    Matrix viewTransform = getViewTransform();
    auto spriteTransform = pSprite->GetTransform();
    auto finalTransform = spriteTransform * viewTransform;
    Vector2 size = {pSprite->GetWidth(), pSprite->GetHeight()};
    auto& align = pSprite->GetAlign();

    std::vector<Vector2> points = {
        Vector2::Transform(Vector2(size.x * -align.x, size.y * -align.y), finalTransform),
        Vector2::Transform(Vector2(size.x * -align.x, size.y * (1.f - align.y)), finalTransform),
        Vector2::Transform(Vector2(size.x * (1.f - align.x), size.y * (1.f - align.y)), finalTransform),
        Vector2::Transform(Vector2(size.x * (1.f - align.x), size.y * -align.y), finalTransform)
    };

    return std::move(points);
}

AABB getSelectionAABB()
{
    Matrix transform = getViewTransform();
    AABB aabb = {Vector2(100000, 100000), Vector2(-100000, -100000)};
    for (auto pContainer : selection)
    {
        auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
        if (!pSprite) continue;

        auto points = getSpriteCorners(pSprite);
        for (auto& point : points)
        {
            aabb[0] = Vector2::Min(aabb[0], point);
            aabb[1] = Vector2::Max(aabb[1], point);
        }
    }
    return std::move(aabb);
}

bool isMultiSelection()
{
    return selection.size() > 1;
}

void updateTransformHandles()
{
    if (isMultiSelection())
    {
        gizmo.transformHandles.resize(8);
        gizmo.aabb = getSelectionAABB();

        gizmo.transformHandles[0].handle = Handle::TOP_LEFT;
        gizmo.transformHandles[0].screenPos = gizmo.aabb[0];
        gizmo.transformHandles[0].transformDirection = Vector2(-1, -1);

        gizmo.transformHandles[1].handle = Handle::LEFT;
        gizmo.transformHandles[1].screenPos = Vector2(gizmo.aabb[0].x, (gizmo.aabb[0].y + gizmo.aabb[1].y) * .5f);
        gizmo.transformHandles[1].transformDirection = Vector2(-1, 0);

        gizmo.transformHandles[2].handle = Handle::BOTTOM_LEFT;
        gizmo.transformHandles[2].screenPos = Vector2(gizmo.aabb[0].x, gizmo.aabb[1].y);
        gizmo.transformHandles[2].transformDirection = Vector2(-1, 1);

        gizmo.transformHandles[3].handle = Handle::BOTTOM;
        gizmo.transformHandles[3].screenPos = Vector2((gizmo.aabb[0].x + gizmo.aabb[1].x) * .5f, gizmo.aabb[1].y);
        gizmo.transformHandles[3].transformDirection = Vector2(0, 1);

        gizmo.transformHandles[4].handle = Handle::BOTTOM_RIGHT;
        gizmo.transformHandles[4].screenPos = gizmo.aabb[1];
        gizmo.transformHandles[4].transformDirection = Vector2(1, 1);

        gizmo.transformHandles[5].handle = Handle::RIGHT;
        gizmo.transformHandles[5].screenPos = Vector2(gizmo.aabb[1].x, (gizmo.aabb[0].y + gizmo.aabb[1].y) * .5f);
        gizmo.transformHandles[5].transformDirection = Vector2(1, 0);

        gizmo.transformHandles[6].handle = Handle::TOP_RIGHT;
        gizmo.transformHandles[6].screenPos = Vector2(gizmo.aabb[1].x, gizmo.aabb[0].y);
        gizmo.transformHandles[6].transformDirection = Vector2(1, -1);

        gizmo.transformHandles[7].handle = Handle::TOP;
        gizmo.transformHandles[7].screenPos = Vector2((gizmo.aabb[0].x + gizmo.aabb[1].x) * .5f, gizmo.aabb[0].y);
        gizmo.transformHandles[7].transformDirection = Vector2(0, -1);

        if (state != State::Rotate)
        {
            selectionCenter = (gizmo.aabb[0] + gizmo.aabb[1]) * .5f;
        }
    }
    else if (selection.empty())
    {
        gizmo.transformHandles.clear();
    }
    else
    {
        auto pSprite = dynamic_cast<seed::Sprite*>(selection.front()->pNode);
        if (!pSprite)
        {
            gizmo.transformHandles.clear();
        }
        else
        {
            gizmo.transformHandles.resize(8);
            auto spriteCorners = getSpriteCorners(pSprite);

            gizmo.transformHandles[0].handle = Handle::TOP_LEFT;
            gizmo.transformHandles[0].screenPos = spriteCorners[0];
            gizmo.transformHandles[0].transformDirection = Vector2(-1, -1);

            gizmo.transformHandles[1].handle = Handle::LEFT;
            gizmo.transformHandles[1].screenPos = (spriteCorners[0] + spriteCorners[1]) * .5f;
            gizmo.transformHandles[1].transformDirection = Vector2(-1, 0);

            gizmo.transformHandles[2].handle = Handle::BOTTOM_LEFT;
            gizmo.transformHandles[2].screenPos = spriteCorners[1];
            gizmo.transformHandles[2].transformDirection = Vector2(-1, 1);

            gizmo.transformHandles[3].handle = Handle::BOTTOM;
            gizmo.transformHandles[3].screenPos = (spriteCorners[1] + spriteCorners[2]) * .5f;
            gizmo.transformHandles[3].transformDirection = Vector2(0, 1);

            gizmo.transformHandles[4].handle = Handle::BOTTOM_RIGHT;
            gizmo.transformHandles[4].screenPos = spriteCorners[2];
            gizmo.transformHandles[4].transformDirection = Vector2(1, 1);

            gizmo.transformHandles[5].handle = Handle::RIGHT;
            gizmo.transformHandles[5].screenPos = (spriteCorners[2] + spriteCorners[3]) * .5f;
            gizmo.transformHandles[5].transformDirection = Vector2(1, 0);

            gizmo.transformHandles[6].handle = Handle::TOP_RIGHT;
            gizmo.transformHandles[6].screenPos = spriteCorners[3];
            gizmo.transformHandles[6].transformDirection = Vector2(1, -1);

            gizmo.transformHandles[7].handle = Handle::TOP;
            gizmo.transformHandles[7].screenPos = (spriteCorners[3] + spriteCorners[0]) * .5f;
            gizmo.transformHandles[7].transformDirection = Vector2(0, -1);

            selectionCenter = Vector2::Transform(pSprite->GetTransform().Translation(), getViewTransform());
        }
    }
}

std::string fileOpen(const char* szFilters)
{
    auto window = OWindow->getHandle();
    char szFileName[MAX_PATH] = "";

    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = window;

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window;
    ofn.lpstrFilter = szFilters;
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "json";

    GetOpenFileNameA(&ofn);

    return ofn.lpstrFile;
}

void changeSpriteProperty(const std::string& actionName, const std::function<void(NodeContainer*)>& logic)
{
    auto pActionGroup = new onut::ActionGroup(actionName);
    for (auto pContainer : selection)
    {
        pContainer->retain();
        SpriteState spriteStateBefore(pContainer);
        logic(pContainer);
        SpriteState spriteStateAfter(pContainer);
        pActionGroup->addAction(new onut::Action("",
            [=]{spriteStateAfter.apply(); updateProperties(); },
            [=]{spriteStateBefore.apply(); updateProperties(); },
            [=]{}, [=]{pContainer->release(); }));
    }
    actionManager.doAction(pActionGroup);
}

void checkAboutToAction(State aboutState, State targetState, const Vector2& mouseDiff)
{
    if (state == aboutState)
    {
        if (mouseDiff.Length() >= 3.f)
        {
            state = targetState;
            for (auto pContainer : selection)
            {
                pContainer->stateOnDown = SpriteState(pContainer);
            }
        }
    }
}

void finalizeAction(const std::string& name, State actionState)
{
    if (state == actionState)
    {
        auto pGroup = new onut::ActionGroup(name);
        for (auto pContainer : selection)
        {
            pContainer->retain();
            auto stateBefore = pContainer->stateOnDown;
            auto stateAfter = SpriteState(pContainer);
            pGroup->addAction(new onut::Action("",
                [=]{
                stateAfter.apply();
                updateProperties();
            }, [=] {
                stateBefore.apply();
                updateProperties();
            }, [=] {
            }, [=] {
                pContainer->release();
            }));
        }
        actionManager.doAction(pGroup);
    }
}

void checkNudge(uintptr_t key)
{
    // Arrow nudge
    float step = OPressed(OINPUT_LCONTROL) ? 10.f : 1.f;
    if (key == VK_LEFT)
    {
        changeSpriteProperty("Nudge", [=](NodeContainer* pContainer)
        {
            SpriteState spriteState(pContainer);
            auto worldPos = Vector2::Transform(spriteState.position, spriteState.parentTransform);
            auto invTransform = spriteState.parentTransform.Invert();
            worldPos.x -= step;
            pContainer->pNode->SetPosition(Vector2::Transform(worldPos, invTransform));
        });
    }
    else if (key == VK_RIGHT)
    {
        changeSpriteProperty("Nudge", [=](NodeContainer* pContainer)
        {
            SpriteState spriteState(pContainer);
            auto worldPos = Vector2::Transform(spriteState.position, spriteState.parentTransform);
            auto invTransform = spriteState.parentTransform.Invert();
            worldPos.x += step;
            pContainer->pNode->SetPosition(Vector2::Transform(worldPos, invTransform));
        });
    }
    if (key == VK_UP)
    {
        changeSpriteProperty("Nudge", [=](NodeContainer* pContainer)
        {
            SpriteState spriteState(pContainer);
            auto worldPos = Vector2::Transform(spriteState.position, spriteState.parentTransform);
            auto invTransform = spriteState.parentTransform.Invert();
            worldPos.y -= step;
            pContainer->pNode->SetPosition(Vector2::Transform(worldPos, invTransform));
        });
    }
    else if (key == VK_DOWN)
    {
        changeSpriteProperty("Nudge", [=](NodeContainer* pContainer)
        {
            SpriteState spriteState(pContainer);
            auto worldPos = Vector2::Transform(spriteState.position, spriteState.parentTransform);
            auto invTransform = spriteState.parentTransform.Invert();
            worldPos.y += step;
            pContainer->pNode->SetPosition(Vector2::Transform(worldPos, invTransform));
        });
    }
}

void init()
{
    createUIStyles(OUIContext);
    OUI->add(OLoadUI("editor.json"));

    pEditingView = new seed::View();
    pEditingView->Show();

    pMainView = OFindUI("mainView");
    pTreeView = dynamic_cast<onut::UITreeView*>(OFindUI("treeView"));
    pPropertyName = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteName"));
    pPropertyClass = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteClass"));
    pPropertyTexture = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteTexture"));
    pPropertyTextureBrowse = dynamic_cast<onut::UIButton*>(OFindUI("btnSpriteTextureBrowse"));
    pPropertyX = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteX"));
    pPropertyY = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteY"));
    pPropertyScaleX = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteScaleX"));
    pPropertyScaleY = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteScaleY"));
    pPropertyAlignX = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteAlignX"));
    pPropertyAlignY = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteAlignY"));
    pPropertyAngle = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteAngle"));
    pPropertyColor = dynamic_cast<onut::UIPanel*>(OFindUI("colSpriteColor"));
    pPropertyAlpha = dynamic_cast<onut::UITextBox*>(OFindUI("txtSpriteAlpha"));

    pPropertyTexture->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Texture", [](NodeContainer* pContainer)
        {
            auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
            if (pSprite)
            {
                pSprite->SetTexture(OGetTexture(pPropertyTexture->textComponent.text.c_str()));
            }
        });
    };
    pPropertyTextureBrowse->onClick = [=](onut::UIControl* pControl, const onut::UIMouseEvent& mouseEvent)
    {
        std::string file = fileOpen("Image Files (*.png)\0*.png\0All Files (*.*)\0*.*\0");
        if (!file.empty())
        {
            // Make it relative to our filename
            pPropertyTexture->textComponent.text = onut::getFilename(file);
            if (pPropertyTexture->onTextChanged)
            {
                onut::UITextBoxEvent evt;
                evt.pContext = OUIContext;
                pPropertyTexture->onTextChanged(pPropertyTexture, evt);
            }
        }
    };
    pPropertyX->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Position X", [](NodeContainer* pContainer)
        {
            pContainer->pNode->SetPosition(Vector2(pPropertyX->getFloat(), pContainer->pNode->GetPosition().y));
        });
    };
    pPropertyY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Position Y", [](NodeContainer* pContainer)
        {
            pContainer->pNode->SetPosition(Vector2(pContainer->pNode->GetPosition().x, pPropertyY->getFloat()));
        });
    };
    pPropertyScaleX->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Scale X", [](NodeContainer* pContainer)
        {
            pContainer->pNode->SetScale(Vector2(pPropertyScaleX->getFloat(), pContainer->pNode->GetScale().y));
        });
    };
    pPropertyScaleY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Scale Y", [](NodeContainer* pContainer)
        {
            pContainer->pNode->SetScale(Vector2(pContainer->pNode->GetScale().x, pPropertyScaleY->getFloat()));
        });
    };
    pPropertyAlignX->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Align X", [](NodeContainer* pContainer)
        {
            auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
            if (pSprite)
            {
                pSprite->SetAlign(Vector2(pPropertyAlignX->getFloat(), pSprite->GetAlign().y));
            }
        });
    };
    pPropertyAlignY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Align Y", [](NodeContainer* pContainer)
        {
            auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
            if (pSprite)
            {
                pSprite->SetAlign(Vector2(pSprite->GetAlign().x, pPropertyAlignY->getFloat()));
            }
        });
    };
    pPropertyAngle->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Angle", [](NodeContainer* pContainer)
        {
            pContainer->pNode->SetAngle(pPropertyAngle->getFloat());
        });
    };
    pPropertyColor->onClick = [=](onut::UIControl* pControl, const onut::UIMouseEvent& evt)
    {
        static COLORREF g_acrCustClr[16]; // array of custom colors

        CHOOSECOLOR colorChooser = {0};
        DWORD rgbCurrent; // initial color selection
        rgbCurrent = (DWORD)pPropertyColor->color.packed;
        rgbCurrent = ((rgbCurrent >> 24) & 0x000000ff) | ((rgbCurrent >> 8) & 0x0000ff00) | ((rgbCurrent << 8) & 0x00ff0000);
        colorChooser.lStructSize = sizeof(colorChooser);
        colorChooser.hwndOwner = OWindow->getHandle();
        colorChooser.lpCustColors = (LPDWORD)g_acrCustClr;
        colorChooser.rgbResult = rgbCurrent;
        colorChooser.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (ChooseColor(&colorChooser) == TRUE)
        {
            onut::sUIColor color;
            rgbCurrent = colorChooser.rgbResult;
            color.packed = ((rgbCurrent << 24) & 0xff000000) | ((rgbCurrent << 8) & 0x00ff0000) | ((rgbCurrent >> 8) & 0x0000ff00) | 0x000000ff;
            color.unpack();
            pPropertyColor->color = color;
            changeSpriteProperty("Change Color", [color](NodeContainer* pContainer)
            {
                auto colorBefore = pContainer->pNode->GetColor();
                pContainer->pNode->SetColor(Color(color.r, color.g, color.g, colorBefore.w));
            });
        }
    };
    pPropertyAlpha->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty("Change Alpha", [](NodeContainer* pContainer)
        {
            auto alpha = pPropertyAlpha->getFloat() / 100.f;
            pContainer->pNode->SetColor(Color(pContainer->pNode->GetColor().x, pContainer->pNode->GetColor().y, pContainer->pNode->GetColor().z, alpha));
        });
    };

    dottedLineAnim.start(0.f, -1.f, .5f, OLinear, OLoop);

    buildMenu();
    OWindow->onMenu = onMenu;
    OWindow->onWrite = [](char c){OUIContext->write(c); };
    OWindow->onKey = [](uintptr_t key)
    {
        if (state != State::Idle) return;
        OUIContext->keyDown(key);
        if (!dynamic_cast<onut::UITextBox*>(OUIContext->getFocusControl()))
        {
            checkNudge(key);
            checkShortCut(key);
        }
    };

    auto mainViewRect = onut::UI2Onut(pMainView->getWorldRect(*OUIContext));
    cameraPos.x = viewSize.x * .5f;
    cameraPos.y = viewSize.y * .5f;

    // Bind toolbox actions
    OFindUI("btnCreateSprite")->onClick = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        if (state != State::Idle) return;

        // Undo/redo
        NodeContainer* pContainer = new NodeContainer();
        auto oldSelection = selection;
        actionManager.doAction(new onut::ActionGroup("Create Sprite",
        {
            new onut::Action("",
                [=]{ // OnRedo
                auto pSprite = pEditingView->AddSprite("default.png");
                pSprite->SetPosition(viewSize * .5f);

                auto pTreeItem = new onut::UITreeViewItem("default.png");
                pTreeItem->pUserData = pContainer;
                pTreeView->addItem(pTreeItem);

                pContainer->pNode = pSprite;
                pContainer->pTreeViewItem = pTreeItem;
                nodesToContainers[pSprite] = pContainer;
            },
                [=]{ // OnUndo
                auto it = nodesToContainers.find(pContainer->pNode);
                if (it != nodesToContainers.end()) nodesToContainers.erase(it);
                pEditingView->DeleteNode(pContainer->pNode);
                pTreeView->removeItem(pContainer->pTreeViewItem);
                pContainer->pTreeViewItem = nullptr;
                pContainer->pNode = nullptr;
            },
                [=]{ // Init
            },
                [=]{ // Destroy
                pContainer->release();
            }),
            new onut::Action("",
                [=]{ // OnRedo
                selection.clear();
                selection.push_back(pContainer);
                updateProperties();
            },
                [=]{ // OnUndo
                selection = oldSelection;
                updateProperties();
            }),
        }));
    };

    // Camera panning
    pMainView->onMiddleMouseDown = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        if (state != State::Idle) return;
        state = State::Panning;
        cameraPosOnDown = cameraPos;
        mousePosOnDown = event.mousePos;
    };
    pMainView->onMouseMove = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        if (state != State::Panning) return;
        Vector2 mouseDiff = Vector2((float)(event.mousePos.x - mousePosOnDown.x), (float)(event.mousePos.y - mousePosOnDown.y));
        mouseDiff /= zoom;
        cameraPos = cameraPosOnDown - mouseDiff;
    };
    pMainView->onMiddleMouseUp = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        if (state != State::Panning) return;
        state = State::Idle;
    };

    // Draw the seed::View
    OUIContext->addStyle<onut::UIPanel>("mainView", [](const onut::UIControl* pControl, const onut::sUIRect& rect)
    {
        OSB->drawRect(nullptr, onut::UI2Onut(rect), OColorHex(232323));
        OSB->end();

        Matrix transform = getViewTransform();
        OSB->begin(transform);
        OSB->drawRect(nullptr, Rect(0, 0, viewSize.x, viewSize.y), Color::Black);
        pEditingView->Render();

        // Draw selection
        const Color DOTTED_LINE_COLOR = {1, 1, 1, .5f};
        const Color AABB_DOTTED_LINE_COLOR = {1, 1, .5f, .5f};
        const Color HANDLE_COLOR = OColorHex(999999);

        auto pDottedLineTexture = OGetTexture("dottedLine.png");
        auto dottedLineScale = 1.f / pDottedLineTexture->getSizef().x * zoom;
        auto dottedLineOffset = dottedLineAnim.get();
        OSB->end();

        for (auto pContainer : selection)
        {
            auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
            if (!pSprite) continue;

            auto points = getSpriteCorners(pSprite);
            Vector2 size = {pSprite->GetWidth(), pSprite->GetHeight()};

            OPB->begin(onut::ePrimitiveType::LINE_STRIP, pDottedLineTexture);
            OPB->draw(points[0], DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset));
            OPB->draw(points[1], DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset + size.y * dottedLineScale * pSprite->GetScale().y));
            OPB->draw(points[2], DOTTED_LINE_COLOR, Vector2(dottedLineOffset + size.x * dottedLineScale * pSprite->GetScale().x, dottedLineOffset + size.y * dottedLineScale * pSprite->GetScale().y));
            OPB->draw(points[3], DOTTED_LINE_COLOR, Vector2(dottedLineOffset + size.x * dottedLineScale * pSprite->GetScale().x, dottedLineOffset));
            OPB->draw(points[0], DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset));
            OPB->end();

            if (!isMultiSelection())
            {
                auto spriteTransform = pSprite->GetTransform();
                OSB->begin(transform);
                OSB->drawCross(spriteTransform.Translation(), 10.f, HANDLE_COLOR);
                OSB->end();
            }
        }
        if (isMultiSelection())
        {
            OPB->begin(onut::ePrimitiveType::LINE_STRIP, pDottedLineTexture);
            OPB->draw(gizmo.aabb[0], AABB_DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset));
            OPB->draw(Vector2(gizmo.aabb[0].x, gizmo.aabb[1].y), AABB_DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset + (gizmo.aabb[1].y - gizmo.aabb[0].y) * dottedLineScale));
            OPB->draw(gizmo.aabb[1], AABB_DOTTED_LINE_COLOR, Vector2(dottedLineOffset + (gizmo.aabb[1].x - gizmo.aabb[0].x) * dottedLineScale, dottedLineOffset + (gizmo.aabb[1].y - gizmo.aabb[0].y) * dottedLineScale));
            OPB->draw(Vector2(gizmo.aabb[1].x, gizmo.aabb[0].y), AABB_DOTTED_LINE_COLOR, Vector2(dottedLineOffset + (gizmo.aabb[1].x - gizmo.aabb[0].x) * dottedLineScale, dottedLineOffset));
            OPB->draw(gizmo.aabb[0], AABB_DOTTED_LINE_COLOR, Vector2(dottedLineOffset, dottedLineOffset));
            OPB->end();
        }

        // Handles
        OSB->begin();
        for (auto& handle : gizmo.transformHandles)
        {
            OSB->drawSprite(nullptr, handle.screenPos, HANDLE_COLOR, 0, 6);
        }
    });

    // Mouse down for select
    pMainView->onMouseDown = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        mousePosOnDown = event.mousePos;

        // Check handles
        if (selection.size() > 0 && !OPressed(OINPUT_LCONTROL))
        {
            HandleIndex index = 0;
            HandleIndex closestIndex = 0;
            auto& closest = gizmo.transformHandles.front();
            float closestDis = Vector2::DistanceSquared(onut::UI2Onut(mousePosOnDown), closest.screenPos);
            for (auto& handle : gizmo.transformHandles)
            {
                float dis = Vector2::DistanceSquared(onut::UI2Onut(mousePosOnDown), handle.screenPos);
                if (dis < closestDis)
                {
                    closest = handle;
                    closestDis = dis;
                    closestIndex = index;
                }
                ++index;
            }
            if (closestDis < 8.f * 8.f && !isMultiSelection())
            {
                state = State::IsAboutToMoveHandle;
                handleIndexOnDown = closestIndex;
            }
            else if (closestDis < 32.f * 32.f)
            {
                state = State::IsAboutToRotate;
            }
        }

        if (state == State::Idle || state == State::IsAboutToRotate)
        {
            // Transform mouse into view
            seed::Node* pMouseHover = nullptr;
            auto mousePosInView = onut::UI2Onut(event.localMousePos);
            auto viewRect = pMainView->getWorldRect(*OUIContext);
            Matrix viewTransform =
                Matrix::CreateTranslation(-cameraPos) *
                Matrix::CreateScale(zoom) *
                Matrix::CreateTranslation(Vector2((float)viewRect.size.x * .5f, (float)viewRect.size.y * .5f));
            auto invViewTransform = viewTransform.Invert();
            mousePosInView = Vector2::Transform(mousePosInView, invViewTransform);

            // Find the topmost mouse hover sprite
            pEditingView->VisitNodesBackward([&](seed::Node* pNode) -> bool
            {
                auto transform = pNode->GetTransform();
                auto invTransform = transform.Invert();
                auto mouseInSprite = Vector2::Transform(mousePosInView, invTransform);
                auto pSprite = dynamic_cast<seed::Sprite*>(pNode);
                if (pSprite)
                {
                    if (mouseInSprite.x >= -pSprite->GetWidth() * pSprite->GetAlign().x &&
                        mouseInSprite.x <= pSprite->GetWidth() * (1.f - pSprite->GetAlign().x) &&
                        mouseInSprite.y >= -pSprite->GetHeight() * pSprite->GetAlign().y &&
                        mouseInSprite.y <= pSprite->GetHeight() * (1.f - pSprite->GetAlign().y))
                    {
                        pMouseHover = pSprite;
                        return true;
                    }
                }
                else
                {
                    if (mouseInSprite.x >= -16 &&
                        mouseInSprite.x <= 16 &&
                        mouseInSprite.y >= -16 &&
                        mouseInSprite.y <= 16)
                    {
                        pMouseHover = pNode;
                        return true;
                    }
                }
                return false;
            });

            // Add to selection
            if (pMouseHover)
            {
                if (OPressed(OINPUT_LCONTROL))
                {
                    auto selectionBefore = selection;
                    auto selectionAfter = selection;
                    bool found = false;
                    for (auto it = selectionAfter.begin(); it != selectionAfter.end(); ++it)
                    {
                        if ((*it)->pNode == pMouseHover)
                        {
                            found = true;
                            selectionAfter.erase(it);
                            break;
                        }
                    }
                    auto pContainer = nodesToContainers[pMouseHover];
                    pContainer->retain();
                    if (!found)
                    {
                        selectionAfter.push_back(pContainer);
                    }
                    actionManager.doAction(new onut::Action("Select",
                        [=]
                    {
                        selection = selectionAfter;
                        updateProperties();
                    }, [=]
                    {
                        selection = selectionBefore;
                        updateProperties();
                    }, [=]
                    {
                    }, [=]
                    {
                        pContainer->release();
                    }));
                }
                else
                {
                    state = State::IsAboutToMove;
                    if (selection.size() == 1)
                    {
                        if (selection.front()->pNode == pMouseHover)
                        {
                            return; // Nothing changed
                        }
                    }
                    bool found = false;
                    for (auto pContainer : selection)
                    {
                        if (pContainer->pNode == pMouseHover)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        auto pContainer = nodesToContainers[pMouseHover];
                        pContainer->retain();
                        auto selectionBefore = selection;
                        auto selectionAfter = selection;
                        selectionAfter.clear();
                        selectionAfter.push_back(pContainer);
                        actionManager.doAction(new onut::Action("Select",
                            [=]
                        {
                            selection = selectionAfter;
                            updateProperties();
                        }, [=]
                        {
                            selection = selectionBefore;
                            updateProperties();
                        }, [=]
                        {
                        }, [=]
                        {
                            pContainer->release();
                        }));
                    }
                }
            }
            else
            {
                // Unselect all if we are not within it's aabb
                if (state != State::IsAboutToRotate)
                {
                    bool doSelection = true;
                    if (isMultiSelection())
                    {
                        auto aabb = getSelectionAABB();
                        if (event.mousePos.x >= aabb[0].x &&
                            event.mousePos.y >= aabb[0].y &&
                            event.mousePos.x <= aabb[1].x &&
                            event.mousePos.y <= aabb[1].y)
                        {
                            doSelection = false;
                        }
                    }
                    if (doSelection)
                    {
                        auto selectionBefore = selection;
                        auto selectionAfter = selection;
                        selectionAfter.clear();
                        actionManager.doAction(new onut::Action("Unselect",
                            [=]
                        {
                            selection = selectionAfter;
                            updateProperties();
                        }, [=]
                        {
                            selection = selectionBefore;
                            updateProperties();
                        }));
                    }
                    else
                    {
                        state = State::IsAboutToMove;
                    }
                }
            }
        }
    };

    pMainView->onMouseMove = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        auto mouseDiff = onut::UI2Onut(event.mousePos) - onut::UI2Onut(mousePosOnDown);
        checkAboutToAction(State::IsAboutToMove, State::Moving, mouseDiff);
        checkAboutToAction(State::IsAboutToMoveHandle, State::MovingHandle, mouseDiff);
        checkAboutToAction(State::IsAboutToRotate, State::Rotate, mouseDiff);
        if (state == State::Moving)
        {
            if (OPressed(OINPUT_LSHIFT))
            {
                if (std::abs(mouseDiff.x) > std::abs(mouseDiff.y))
                {
                    mouseDiff.y = 0;
                }
                else
                {
                    mouseDiff.x = 0;
                }
            }
            for (auto pContainer : selection)
            {
                auto transform = pContainer->stateOnDown.parentTransform;
                auto worldPos = Vector2::Transform(pContainer->stateOnDown.position, transform);
                auto invTransform = transform.Invert();
                worldPos += mouseDiff;
                pContainer->pNode->SetPosition(Vector2::Transform(worldPos, invTransform));
            }
            updateProperties();
        }
        else if (state == State::MovingHandle)
        {
            auto& handle = gizmo.transformHandles[handleIndexOnDown];
            auto pContainer = selection.front();
            auto pSprite = dynamic_cast<seed::Sprite*>(pContainer->pNode);
            if (pSprite)
            {
                auto invTransform = pContainer->stateOnDown.transform.Invert();
                invTransform._41 = 0;
                invTransform._42 = 0;
                auto size = Vector2(pSprite->GetWidth(), pSprite->GetHeight());

                auto localMouseDiff = Vector2::Transform(mouseDiff, invTransform);
                auto localScaleDiff = handle.transformDirection * localMouseDiff;
                if (OPressed(OINPUT_LSHIFT))
                {
                    localScaleDiff.x = localScaleDiff.y = std::max<>(localScaleDiff.x, localScaleDiff.y);
                }
                auto newScale = pContainer->stateOnDown.scale + localScaleDiff / size * 2.f * pContainer->stateOnDown.scale;

                pSprite->SetScale(newScale);
                updateProperties();
            }
        }
        else if (state == State::Rotate)
        {
            auto diff1 = onut::UI2Onut(mousePosOnDown) - selectionCenter;
            auto diff2 = onut::UI2Onut(event.mousePos) - selectionCenter;
            auto angle1 = DirectX::XMConvertToDegrees(std::atan2f(diff1.y, diff1.x));
            auto angle2 = DirectX::XMConvertToDegrees(std::atan2f(diff2.y, diff2.x));
            auto angleDiff = angle2 - angle1;
            if (OPressed(OINPUT_LSHIFT))
            {
                angleDiff = std::round(angleDiff / 5.f) * 5.f;
            }
            if (isMultiSelection())
            {
                auto viewTransform = getViewTransform();
                auto invViewTransform = viewTransform.Invert();
                for (auto pContainer : selection)
                {
                    auto screenPosition = Vector2::Transform(pContainer->stateOnDown.transform.Translation(), viewTransform);
                    auto centerVect = screenPosition - selectionCenter;
                    centerVect = Vector2::Transform(centerVect, Matrix::CreateRotationZ(DirectX::XMConvertToRadians(angleDiff)));
                    screenPosition = centerVect + selectionCenter;
                    auto viewPosition = Vector2::Transform(screenPosition, invViewTransform);
                    auto invParentTransform = pContainer->stateOnDown.parentTransform.Invert();
                    auto localPosition = Vector2::Transform(viewPosition, invParentTransform);
                    pContainer->pNode->SetPosition(localPosition);
                    pContainer->pNode->SetAngle(pContainer->stateOnDown.angle + angleDiff);
                }
            }
            else
            {
                auto pContainer = selection.front();
                pContainer->pNode->SetAngle(pContainer->stateOnDown.angle + angleDiff);
            }
            updateProperties();
        }
    };

    pMainView->onMouseUp = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        finalizeAction("Move", State::Moving);
        finalizeAction("Scale", State::MovingHandle);
        finalizeAction("Rotate", State::Rotate);
        state = State::Idle;
    };

    updateProperties();
}

void update()
{
    if (state == State::Idle)
    {
        // Zoom
        if (OUIContext->getHoverControl() == pMainView)
        {
            if (OInput->getStateValue(OINPUT_MOUSEZ) < 0)
            {
                --zoomIndex;
                if (zoomIndex < 0) zoomIndex = 0;
                zoom = zoomLevels[zoomIndex];
            }
            else if (OInput->getStateValue(OINPUT_MOUSEZ) > 0)
            {
                ++zoomIndex;
                if (zoomIndex > (ZoomIndex)zoomLevels.size() - 1) zoomIndex = (ZoomIndex)zoomLevels.size() - 1;
                zoom = zoomLevels[zoomIndex];
            }
        }
    }
}

void render()
{
}
