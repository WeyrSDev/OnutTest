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
    Zooming
};

class SpriteContainer : public onut::Object
{
public:
    SpriteContainer() { retain(); }
    seed::Sprite* pSprite = nullptr;
    onut::UITreeViewItem* pTreeViewItem = nullptr;
};

struct SpriteState
{
    std::string texture;
    Vector2 position;
    Vector2 scale;
    float angle;
    Color color;
    Vector2 align;
    SpriteContainer* pContainer = nullptr;

    SpriteState() {}

    SpriteState(const SpriteState& copy)
    {
        texture = copy.texture;
        position = copy.position;
        scale = copy.scale;
        angle = copy.angle;
        color = copy.color;
        align = copy.align;
        pContainer = copy.pContainer;
    }

    SpriteState(SpriteContainer* in_pContainer)
    {
        pContainer = in_pContainer;
        auto pSprite = pContainer->pSprite;

        texture = pSprite->GetTexture()->getName();
        position = pSprite->GetPosition();
        scale = pSprite->GetScale();
        angle = pSprite->GetAngle();
        color = pSprite->GetColor();
        align = pSprite->GetAlign();
    }

    void apply() const
    {
        if (!pContainer) return;
        auto pSprite = pContainer->pSprite;
        if (!pSprite) return;

        pSprite->SetTexture(OGetTexture(texture.c_str()));
        pSprite->SetPosition(position);
        pSprite->SetScale(scale);
        pSprite->SetAngle(angle);
        pSprite->SetColor(color);
        pSprite->SetAlign(align);
    }
};

// Utilities
onut::ActionManager actionManager;

// Camera
using Zoom = float;
using ZoomIndex = int;
static const std::vector<Zoom> zoomLevels = {.20f, .50f, .70f, 1.f, 1.5f, 2.f, 4.f};
ZoomIndex zoomIndex = 3;
Zoom zoom = zoomLevels[zoomIndex];
Vector2 cameraPos = Vector2::Zero;

// Selection
using Selection = std::vector<SpriteContainer*>;
Selection selection;
OAnimf dottedLineAnim = 0.f;
std::unordered_map<seed::Sprite*, onut::UITreeViewItem*> spritesToTreeViewItem;

// State
State state = State::Idle;
Vector2 cameraPosOnDown;
onut::sUIVector2 mousePosOnDown;

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

    for (auto pContainer : selection)
    {
        pPropertyName->isEnabled = true;
        pPropertyClass->isEnabled = true;
        pPropertyTexture->isEnabled = true;
        pPropertyTextureBrowse->isEnabled = true;
        pPropertyX->isEnabled = true;
        pPropertyY->isEnabled = true;
        pPropertyScaleX->isEnabled = true;
        pPropertyScaleY->isEnabled = true;
        pPropertyAlignX->isEnabled = true;
        pPropertyAlignY->isEnabled = true;
        pPropertyAngle->isEnabled = true;
        pPropertyColor->isEnabled = true;
        pPropertyAlpha->isEnabled = true;

        pPropertyName->textComponent.text = "";
        pPropertyClass->textComponent.text = "";
        auto pTexture = pContainer->pSprite->GetTexture();
        if (pTexture)
        {
            pPropertyTexture->textComponent.text = pTexture->getName();
        }
        else
        {
            pPropertyTexture->textComponent.text = "";
        }
        pPropertyX->setFloat(pContainer->pSprite->GetPosition().x);
        pPropertyY->setFloat(pContainer->pSprite->GetPosition().y);
        pPropertyScaleX->setFloat(pContainer->pSprite->GetScale().x);
        pPropertyScaleY->setFloat(pContainer->pSprite->GetScale().y);
        pPropertyAlignX->setFloat(pContainer->pSprite->GetAlign().x);
        pPropertyAlignY->setFloat(pContainer->pSprite->GetAlign().y);
        pPropertyAngle->setFloat(pContainer->pSprite->GetAngle());
        pPropertyColor->color = onut::sUIColor(pContainer->pSprite->GetColor().x, pContainer->pSprite->GetColor().y, pContainer->pSprite->GetColor().z, pContainer->pSprite->GetColor().w);
        pPropertyAlpha->setFloat(pContainer->pSprite->GetColor().w * 100.f);
    }
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

void changeSpriteProperty(const std::function<void(SpriteContainer*)>& logic)
{
    auto pActionGroup = new onut::ActionGroup("Move Sprite");
    for (auto pContainer : selection)
    {
        SpriteState spriteStateBefore(pContainer);
        logic(pContainer);
        SpriteState spriteStateAfter(pContainer);
        pActionGroup->addAction(new onut::Action("",
            [=]{spriteStateAfter.apply(); updateProperties(); },
            [=]{spriteStateBefore.apply(); updateProperties(); }));
    }
    actionManager.doAction(pActionGroup);
}

void init()
{
    createUIStyles(OUIContext);
    OUI->add(OLoadUI("editor.json"));

    pEditingView = new seed::View();

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
        for (auto pContainer : selection) pContainer->pSprite->SetTexture(OGetTexture(pPropertyTexture->textComponent.text.c_str()));
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
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            pContainer->pSprite->SetPosition(Vector2(pPropertyX->getFloat(), pContainer->pSprite->GetPosition().y));
        });
    };
    pPropertyY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetPosition(Vector2(pContainer->pSprite->GetPosition().x, pPropertyY->getFloat()));
        });
    };
    pPropertyScaleX->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetScale(Vector2(pPropertyScaleX->getFloat(), pContainer->pSprite->GetScale().y));
        });
    };
    pPropertyScaleY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetScale(Vector2(pContainer->pSprite->GetScale().x, pPropertyScaleY->getFloat()));
        });
    };
    pPropertyAlignX->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetAlign(Vector2(pPropertyAlignX->getFloat(), pContainer->pSprite->GetAlign().y));
        });
    };
    pPropertyAlignY->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetAlign(Vector2(pContainer->pSprite->GetAlign().x, pPropertyAlignY->getFloat()));
        });
    };
    pPropertyAngle->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            for (auto pContainer : selection) pContainer->pSprite->SetAngle(pPropertyAngle->getFloat());
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
            changeSpriteProperty([color](SpriteContainer* pContainer)
            {
                auto colorBefore = pContainer->pSprite->GetColor();
                pContainer->pSprite->SetColor(Color(color.r, color.g, color.g, colorBefore.w));
            });
        }
    };
    pPropertyAlpha->onTextChanged = [](onut::UITextBox* pControl, const onut::UITextBoxEvent& event)
    {
        changeSpriteProperty([](SpriteContainer* pContainer)
        {
            auto alpha = pPropertyAlpha->getFloat() / 100.f;
            for (auto pContainer : selection) pContainer->pSprite->SetColor(Color(pContainer->pSprite->GetColor().x, pContainer->pSprite->GetColor().y, pContainer->pSprite->GetColor().z, alpha));
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
        SpriteContainer* pContainer = new SpriteContainer();
        auto* pSelection = new Selection(selection);
        actionManager.doAction(new onut::ActionGroup("Create Sprite",
        {
            new onut::Action("",
                [=]{ // OnRedo
                auto pSprite = pEditingView->AddSprite("default.png");
                pSprite->SetPosition(viewSize * .5f);

                auto pTreeItem = new onut::UITreeViewItem("default.png");
                pTreeItem->pUserData = pContainer;
                pTreeView->addItem(pTreeItem);

                pContainer->pSprite = pSprite;
                pContainer->pTreeViewItem = pTreeItem;

                spritesToTreeViewItem[pSprite] = pTreeItem;
            },
                [=]{ // OnUndo
                auto it = spritesToTreeViewItem.find(pContainer->pSprite);
                if (it != spritesToTreeViewItem.end()) spritesToTreeViewItem.erase(it);
                pEditingView->DeleteNode(pContainer->pSprite);
                pTreeView->removeItem(pContainer->pTreeViewItem);
                pContainer->pTreeViewItem = nullptr;
                pContainer->pSprite = nullptr;
            },
                [=]{ // Init
            },
                [=]{ // Destroy
                delete pContainer;
            }),
            new onut::Action("",
                [=]{ // OnRedo
                selection.clear();
                selection.push_back(pContainer);
                updateProperties();
            },
                [=]{ // OnUndo
                selection = *pSelection;
                updateProperties();
            },
                [=]{ // Init
            },
                [=]{ // Destroy
                delete pSelection;
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

        Matrix transform =
            Matrix::CreateTranslation(-cameraPos) *
            Matrix::CreateScale(zoom) *
            Matrix::CreateTranslation(Vector2((float)rect.position.x + (float)rect.size.x * .5f, (float)rect.position.y + (float)rect.size.y * .5f));
        OSB->begin(transform);
        OSB->drawRect(nullptr, Rect(0, 0, viewSize.x, viewSize.y), Color::Black);
        pEditingView->Render();

        // Draw selection
        const Color DOTTED_LINE_COLOR = {1, 1, 1, .5f};

        auto pDottedLineTexture = OGetTexture("dottedLine.png");
        auto dottedLineScale = 1.f / pDottedLineTexture->getSizef().x * zoom;
        auto dottedLineOffset = dottedLineAnim.get();
        OSB->end();

        for (auto pContainer : selection)
        {
            auto spriteTransform = pContainer->pSprite->GetTransform();
            Vector2 size = {pContainer->pSprite->GetWidth(), pContainer->pSprite->GetHeight()};

            if (dynamic_cast<seed::Sprite*>(pContainer->pSprite))
            {
                auto& align = pContainer->pSprite->GetAlign();

                OPB->begin(onut::ePrimitiveType::LINE_STRIP, pDottedLineTexture, spriteTransform * transform);
                OPB->draw(Vector2(size.x * -align.x, size.y * -align.y), DOTTED_LINE_COLOR, 
                          Vector2(dottedLineOffset, dottedLineOffset));
                OPB->draw(Vector2(size.x * -align.x, size.y * (1.f - align.y)), DOTTED_LINE_COLOR, 
                          Vector2(dottedLineOffset, dottedLineOffset + size.y * dottedLineScale * pContainer->pSprite->GetScale().y));
                OPB->draw(Vector2(size.x * (1.f - align.x), size.y * (1.f - align.y)), DOTTED_LINE_COLOR, 
                          Vector2(dottedLineOffset + size.x * dottedLineScale * pContainer->pSprite->GetScale().x, dottedLineOffset + size.y * dottedLineScale * pContainer->pSprite->GetScale().y));
                OPB->draw(Vector2(size.x * (1.f - align.x), size.y * -align.y), DOTTED_LINE_COLOR, 
                          Vector2(dottedLineOffset + size.x * dottedLineScale * pContainer->pSprite->GetScale().x, dottedLineOffset));
                OPB->draw(Vector2(size.x * -align.x, size.y * -align.y), DOTTED_LINE_COLOR, 
                          Vector2(dottedLineOffset, dottedLineOffset));
                OPB->end();

                OSB->begin(transform);
                OSB->drawCross(spriteTransform.Translation(), 10.f, DOTTED_LINE_COLOR);
                OSB->end();
            }
            else
            {
            }
        }
        OSB->begin();
    });

    // Mouse down for select
    pMainView->onClick = [](onut::UIControl* pControl, const onut::UIMouseEvent& event)
    {
        auto viewRect = pControl->getWorldRect(*OUIContext);
        seed::Sprite* pMouseHover = nullptr;
        auto mousePosInView = onut::UI2Onut(event.localMousePos);
        Matrix viewTransform =
            Matrix::CreateTranslation(-cameraPos) *
            Matrix::CreateScale(zoom) *
            Matrix::CreateTranslation(Vector2((float)viewRect.size.x * .5f, (float)viewRect.size.y * .5f));
        auto invViewTransform = viewTransform.Invert();
        mousePosInView = Vector2::Transform(mousePosInView, invViewTransform);

        // Find the topmost mouse hover sprite
        pEditingView->visitNodesBackward([&](seed::Node* pNode) -> bool
        {
            auto pSprite = dynamic_cast<seed::Sprite*>(pNode);
            if (pSprite)
            {
                auto transform = pNode->GetTransform();
                auto invTransform = transform.Invert();
                auto mouseInSprite = Vector2::Transform(mousePosInView, invTransform);
                if (mouseInSprite.x >= -pSprite->GetWidth() * pSprite->GetAlign().x &&
                    mouseInSprite.x <= pSprite->GetWidth() * (1.f - pSprite->GetAlign().x) &&
                    mouseInSprite.y >= -pSprite->GetHeight() * pSprite->GetAlign().y &&
                    mouseInSprite.y <= pSprite->GetHeight() * (1.f - pSprite->GetAlign().y))
                {
                    pMouseHover = pSprite;
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
                    if ((*it)->pSprite == pMouseHover)
                    {
                        found = true;
                        selectionAfter.erase(it);
                        break;
                    }
                }
                auto pContainer = new SpriteContainer();
                pContainer->pSprite = pMouseHover;
                pContainer->pTreeViewItem = spritesToTreeViewItem[pContainer->pSprite];
                if (!found)
                {
                    selectionAfter.push_back(pContainer);
                }
                actionManager.doAction(new onut::Action("Select",
                    [=]
                {
                    selection = selectionAfter;
                }, [=]
                {
                    selection = selectionBefore;
                }, [=]
                {
                }, [=]
                {
                    delete pContainer;
                }));
            }
            else
            {
                if (selection.size() == 1)
                {
                    if (selection.front()->pSprite == pMouseHover) return; // Nothing changed
                }
                auto pContainer = new SpriteContainer();
                pContainer->pSprite = pMouseHover;
                pContainer->pTreeViewItem = spritesToTreeViewItem[pContainer->pSprite];
                auto selectionBefore = selection;
                auto selectionAfter = selection;
                selectionAfter.clear();
                selectionAfter.push_back(pContainer);
                actionManager.doAction(new onut::Action("Select",
                    [=]
                {
                    selection = selectionAfter;
                }, [=]
                {
                    selection = selectionBefore;
                }, [=]
                {
                }, [=]
                {
                    delete pContainer;
                }));
            }
        }
        else
        {
            // Unselect all
            auto selectionBefore = selection;
            auto selectionAfter = selection;
            selectionAfter.clear();
            actionManager.doAction(new onut::Action("Unselect",
                [=]
            {
                selection = selectionAfter;
            }, [=]
            {
                selection = selectionBefore;
            }));
        }
    };

    updateProperties();
}

void update()
{
    if (state == State::Idle)
    {
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
