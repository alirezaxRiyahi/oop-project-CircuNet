// --- SDL2 ---
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>        // برای SDL_SysWMinfo
#include <SDL2/SDL2_gfx.h>  // gfx واقعی

// --- STL ---
#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <complex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>

// --- Windows TCP/IP ---
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

// --- Windows GUI ---
#include <windows.h>
#include <commdlg.h>

// --- Cereal ---
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/complex.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/utility.hpp>

using namespace std;

//////---------------------------------------
int WindowH=768;
int WindowW=1366;
int stepX=15;
int stepY=15;
string nameFile="firstRun";
string IP="127.0.0.1";
int PORT=8080;
bool isOnceSave=false;

//////---------------------------------------
void snapToGrid(int &x, int &y) {
    x = (x / stepX) * stepX + stepX/2;  // قرارگیری دقیقاً روی گره‌های مرکزی
    y = (y / stepY) * stepY + stepY/2;
}
//////---------------------------------------
namespace Exceptions {
    class SyntaxError:public exception{
        const char* what() const noexcept override {
            return "Syntax error";
        }
    };
    class groundNodeNotFound : public exception{
        const char* what() const noexcept override {
            return "Node does not exist";
        }
    };
    class ErrorInput : public exception {
    public:
        const char* what() const noexcept override {
            return "-Error : Inappropriate input";
        }
    };
    class elementNotFound:public exception{
    public:
        string message;
        explicit elementNotFound(const std::string& type) {
            message = "Error: Cannot delete " + type + "; component not found";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class elementNotFound2 : public exception {
    public:
        string message;
        explicit elementNotFound2(const std::string& name) {
            message = "ERROR: Element \"" + name + "\" does not exist in the circuit";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class invalidValue:public exception{
    public:
        string message;
        explicit invalidValue(const std::string& type) {
            message = "Error: "+type+" cannot be zero or negative";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class notFoundInLibrary:public exception{
    public:
        string message;
        explicit notFoundInLibrary(const std::string& type) {
            message ="Error: Element "+type+" not found in library";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class duplicateElementName : public exception {
    public:
        string message;
        explicit duplicateElementName(const std::string& type,const std::string& name) {
            message = "Error: "+type+" "+ name + " already exists in the circuit";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class nodeNotFound : public exception {
    public:
        string message;
        explicit nodeNotFound(const std::string& old_name) {
            message = "ERROR: Node "+old_name+" does not exist in the circuit";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class duplicateNodeName : public exception {
    public:
        string message;
        explicit duplicateNodeName(const std::string& name) {
            message ="ERROR: Node name "+name+" already exists";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class diodeModelNotFound : public exception {
    public:
        string message;
        explicit diodeModelNotFound(const std::string& name) {
            message ="Error: Model "+name+" not found in library";
        }
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
    class NotFoundGND:public exception{
        const char* what() const noexcept override {
            return "Error: Number of grounds must not be 0 or more than one";
        }
    };
    class DiscontinuousCircuit:public exception{
        const char* what() const noexcept override {
            return "The circuit is not connected.";
        }
    };
}
using namespace Exceptions;
//////---------------------------------------
namespace Circuit{
    class Node{
    private:
        double voltage=0;
        bool isGround = false;
    public:
        int x,y;
        complex<double>AcVoltage;
        string name;
        SDL_Rect rect;
        SDL_Rect expandedRect;
        bool enabled= false,visible= false,pressed= false;
        SDL_Color color{255, 0, 0, 255}; // رنگ پیش‌فرض قرمز

        Node() : x(0), y(0), name("N0"), rect{0, 0, 2, 2} {
            // سازنده پیش‌فرض
            voltage=0.0;
            isGround= false;
        }

        Node(int x, int y): x(x),y(y) {
            rect = {x, y, 3, 3}; // اندازه پیش‌فرض برای مقاومت
            expandedRect = {
                    rect.x - 5,      // ۵ پیکسل به چپ
                    rect.y - 5,      // ۵ پیکسل به بالا
                    rect.w + 10,     // ۱۰ پیکسل به عرض (۵ از هر طرف)
                    rect.h + 10      // ۱۰ پیکسل به ارتفاع (۵ از هر طرف)
            };
            name = "N" + to_string((y / stepY) * (WindowH / stepY) + (x / stepX) + 1);
            voltage=0.0;
            isGround= false;
        }

        // تابع serialize
        template<class Archive>
        void serialize(Archive & ar)
        {
            ar(x, y, voltage, isGround, AcVoltage, name);
            ar(rect.x, rect.y, rect.w, rect.h);
            ar(enabled, visible, pressed);
            ar(color.r, color.g, color.b, color.a);
            ar(expandedRect.x,expandedRect.y,expandedRect.w,expandedRect.h);
        }

        void handleEvent(const SDL_Event& e) {
            if (!enabled) return;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Point p{ e.button.x, e.button.y };
                if (SDL_PointInRect(&p, &expandedRect)) pressed = true;
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Point p{ e.button.x, e.button.y };
                bool wasPressed = pressed;
                pressed = false;

// ایجاد مستطیل با ۵ پیکسل حاشیه در هر طرف


                bool inside = SDL_PointInRect(&p, &expandedRect);

                if (inside && wasPressed) {
                    visible = true;
                } else {
                    visible = false;
                }
            }
        }

        void draw(SDL_Renderer* renderer, TTF_Font* font) {
            // رسم مستطیل اصلی
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &rect);

            // رسم حاشیه
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &rect);

            if (visible) {
                // رسم نام و مقدار
                string displayText = name;
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, displayText.c_str(), textColor);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        // تعیین اندازه مستطیل متن (بزرگتر از متن اصلی)
                        int textPadding = 10; // فضای اضافه اطراف متن
                        int textBoxWidth = surf->w + textPadding * 2;
                        int textBoxHeight = surf->h + textPadding * 2;

                        // تعیین موقعیت مستطیل متن (گوشه بالا سمت راست)
                        SDL_Rect textBoxRect{
                                rect.x + rect.w - textBoxWidth, // موقعیت X در گوشه راست
                                rect.y, // موقعیت Y در بالای گره
                                textBoxWidth,
                                textBoxHeight
                        };

                        // رسم مستطیل پس‌زمینه متن
                        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); // رنگ خاکستری تیره
                        SDL_RenderFillRect(renderer, &textBoxRect);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderDrawRect(renderer, &textBoxRect);

                        // موقعیت متن در مرکز مستطیل متن
                        SDL_Rect textRect{
                                textBoxRect.x + (textBoxRect.w - surf->w) / 2,
                                textBoxRect.y + (textBoxRect.h - surf->h) / 2,
                                surf->w,
                                surf->h
                        };

                        SDL_RenderCopy(renderer, tex, nullptr, &textRect);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
        }
        //phase 1
        string getName() const { return name; }
        double getVoltage() const { return voltage; }
        void setName(const string& n) { name = n; }
        void setVoltage(double v) { voltage = v; }
        bool getIsGround (){
            return isGround;
        }
        void setIsGround (bool state){
            isGround=state;
        }
    };

    class liine {
        string randomName() {
            return "N0" + to_string(count++);
        }
    public:
        static int count;
        shared_ptr<Node> startNode;
        shared_ptr<Node> endNode;
        int A = 0, B = 0, C = 0;
        string lineName;
        bool isDrawing = false;
        bool isComplete = false;
        vector<shared_ptr<Node>> Nodes;
    private:
        bool waitingForFirstClick = true;
        int x1 = 0, y1 = 0;
        int x2 = 0, y2 = 0;

    public:
        template<class Archive>
        void serialize(Archive &ar) {
            ar(startNode, endNode, A, B, C,
               lineName, isDrawing, isComplete,
               Nodes, waitingForFirstClick,
               x1, y1, x2, y2);
        }
        liine(){
            lineName = randomName();
        }
        void handleEvent(const SDL_Event& e) {

        }

        void draw(SDL_Renderer* renderer) {
            if (isDrawing) {
                // رسم خط موقت هنگام کشیدن
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // رنگ قرمز برای خط موقت
                SDL_RenderDrawLine(renderer, startNode->x+1, startNode->y+1, mouseX+1, mouseY+1);
            }

            if (isComplete) {
                // رسم خط نهایی
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // رنگ آبی برای خط نهایی
                SDL_RenderDrawLine(renderer, startNode->x+1, startNode->y+1, endNode->x+1, endNode->y+1);
                A = endNode->y -startNode->y;
                B = endNode->x - startNode->x;
                C = startNode->x*endNode->y + endNode->y * startNode->x;
            }

        }
    };

    int liine::count=1;

    class Wire {
    private:
        shared_ptr<Node> tempStart; // نقطه شروع خط موقت
        shared_ptr<Node> tempEnd;  // نقطه پایان خط موقت
    public:
        template <class Archive>
        void serialize(Archive& archive) {
            archive(tempStart, tempEnd, isActive, componentId); // بدون نام
        }
        Wire() : isActive(false), tempStart(nullptr), tempEnd(nullptr) {}

        static shared_ptr<Node> findNode(int x, int y) {
            for (auto &i: allNodes) {
                if (i->x == x && i->y == y)
                    return i;
            }
            return nullptr;
        }

        static vector<shared_ptr<Node>> findNodeWhitname(string name) {
            vector<shared_ptr<Node>>similarName;
            for (shared_ptr<Node>&i: allNodes) {
                if (i->getName() == name) { similarName.push_back(i); }
            }
            return similarName;
        }

        static vector<shared_ptr<Node>> allNodes;
        static vector<shared_ptr<liine>> Lines;
        bool isActive;
        int componentId = 1;
        ///----------
        void findPointsOnLine(int x0, int y0, int x1, int y1,std::vector<shared_ptr<Node>>& result)
        {
            bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
            if (steep) {
                std::swap(x0, y0);
                std::swap(x1, y1);
            }
            if (x0 > x1) {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }

            int dx = x1 - x0;
            int dy = std::abs(y1 - y0);
            int error = dx / 2;
            int ystep = (y0 < y1) ? 1 : -1;
            int y = y0;

            for (int x = x0; x <= x1; x++) {
                int px = steep ? y : x;
                int py = steep ? x : y;

                for (auto& node : allNodes) {
                    if (node && node->x == px && node->y == py) {
                        node->enabled= true;
                        result.push_back(node);
                        break;
                    }
                }

                error -= dy;
                if (error < 0) {
                    y += ystep;
                    error += dx;
                }
            }
        };


        void newCircuit() {
            componentId = 1;
// ساخت گراف اتصالات کامل (شامل نقاط ابتدا، انتها و میانی)
            std::unordered_map<std::string, std::unordered_set<std::string>> connectionGraph;

// پر کردن گراف اتصالات با در نظر گرفتن تمام نقاط
            for (auto& line : Lines) {
                if (!line || line->Nodes.empty()) continue;

                // اتصال هر نقطه به نقطه بعدی در خط
                for (size_t i = 0; i < line->Nodes.size() - 1; ++i) {
                    if (!line->Nodes[i] || !line->Nodes[i+1]) continue;

                    std::string currentName = line->Nodes[i]->name;
                    std::string nextName = line->Nodes[i+1]->name;

                    connectionGraph[currentName].insert(nextName);
                    connectionGraph[nextName].insert(currentName);
                }
            }

// یافتن کامپوننت‌های متصل
            std::unordered_map<std::string, std::string> componentMap;
            std::unordered_set<std::string> visited;


            for (auto& entry : connectionGraph) {
                const std::string& nodeName = entry.first;

                if (visited.find(nodeName) != visited.end()) continue;

                std::queue<std::string> q;
                q.push(nodeName);
                visited.insert(nodeName);

                std::string componentName = "N0" + std::to_string(componentId++);

                while (!q.empty()) {
                    std::string current = q.front();
                    q.pop();

                    componentMap[current] = componentName;

                    for (const std::string& neighbor : connectionGraph[current]) {
                        if (visited.find(neighbor) == visited.end()) {
                            visited.insert(neighbor);
                            q.push(neighbor);
                        }
                    }
                }
            }
            // ساخت مجموعه گره‌های متصل به سیم‌ها (برای جستجوی سریع)
            std::unordered_set<shared_ptr<Node>> wiredNodesSet;
            for (auto& line : Lines) {
                for (auto& node : line->Nodes) {
                    if (node) {
                        wiredNodesSet.insert(node);
                    }
                }
            }
            // اعمال نام‌گذاری با ترتیب مشابه کد اصلی
            for (auto& node : allNodes) {
                if (!node) continue;

                // بررسی وجود گره در سیم‌ها (با همان منطق قبلی)
                bool isWired = (wiredNodesSet.find(node) != wiredNodesSet.end());

                if (!isWired) {
                    // نام‌گذاری برای گره‌های بدون سیم (مشابه قبل)
                    node->name = "N" + to_string((node->y / stepY) * (WindowH / stepY) + (node->x / stepX) + 1);
                }
                else {
                    // نام‌گذاری برای گره‌های متصل به سیم
                    auto it = componentMap.find(node->name);
                    if (it != componentMap.end()) {
                        node->name = it->second;
                        node->setIsGround(false);
                    }
                }
            }
        }

        void deleteline() {
            for (int i = 0; i < Lines.size(); ++i) {
                shared_ptr<liine> line = Lines[i];

                // حذف خطوط نال یا بدون نقطه شروع
                if (!line || !line->startNode) {
                    //cout << "0 ";
                    Lines.erase(Lines.begin() + i);
                    i--;
                    continue;
                }

                // بررسی خطوط کامل
                if (line->isComplete) {
                    // نقطه شروع خارج از محدوده
                    if (line->startNode->x < 0 || line->startNode->x > WindowW ||
                        line->startNode->y < 0 || line->startNode->y > WindowH) {
                        //cout << "1 ";
                        Lines.erase(Lines.begin() + i);
                        i--;
                        continue;
                    }

                    // نقطه پایان خارج از محدوده
                    if (!line->endNode || line->endNode->x < 0 || line->endNode->x > WindowW ||
                        line->endNode->y < 0 || line->endNode->y > WindowH) {
                        //cout << "2 ";
                        Lines.erase(Lines.begin() + i);
                        i--;
                        continue;
                    }

                    // شروع و پایان یکسان
                    if (line->startNode->x == line->endNode->x &&
                        line->startNode->y == line->endNode->y) {
                        //cout << "3 ";
                        Lines.erase(Lines.begin() + i);
                        i--;
                        continue;
                    }
                }
                    // خطوط ناقص با نودهای خارج از محدوده
                else {
                    if (line->startNode->x < 0 || line->startNode->x > WindowW ||
                        line->startNode->y < 0 || line->startNode->y > WindowH||
                        line->endNode->x < 0 || line->endNode->x > WindowW ||
                        line->endNode->y < 0 || line->endNode->y > WindowH
                            ) {
                        //cout << "4 ";
                        Lines.erase(Lines.begin() + i);
                        i--;
                        continue;
                    }
                }
            }
        }

        ///--------------
        void toggleWireMode() {
            isActive = !isActive;
            if (!isActive) {
                // اگر حالت وایر غیرفعال شد، خط موقت را پاک کن
                tempStart = nullptr;
                tempEnd = nullptr;
            }
        }

        void handleMouseClick(int x, int y) {
            if (!isActive) return;

            snapToGrid(x, y);
            shared_ptr<Node>clickedNode = findNode(x, y);

            if (!tempStart) {
                // کلیک اول - نقطه شروع
                tempStart = clickedNode;
            } else {
                // کلیک دوم - نقطه پایان
                auto newLine = make_shared<liine>();
                newLine->startNode = tempStart;
                newLine->endNode = clickedNode;
                newLine->isComplete = true;

                // محاسبات اضافی برای خط
                newLine->A = newLine->endNode->x - newLine->startNode->x;
                newLine->B = newLine->endNode->y - newLine->startNode->y;
                newLine->C = -newLine->B * newLine->startNode->x + newLine->A * newLine->startNode->y;

                // پیدا کردن نقاط روی خط
                findPointsOnLine(newLine->startNode->x, newLine->startNode->y,
                                 newLine->endNode->x, newLine->endNode->y,
                                 newLine->Nodes);

                // نامگذاری نقاط
                for (auto node: newLine->Nodes) {
                    if (node) node->name = newLine->lineName;
                    node->enabled= true;
                }

                Lines.push_back(newLine);
                //newCircuit();
                // ریست برای خط بعدی
                tempStart = nullptr;
                tempEnd = nullptr;
            }
        }

        void updateMousePosition(int x, int y) {
            if (!isActive || !tempStart) return;

            snapToGrid(x, y);
            tempEnd = findNode(x, y);
        }

        void draw(SDL_Renderer *ren) {
            // رسم خطوط کامل شده
            for (auto &line: Lines) {
                if (line && line->isComplete) {
                    line->draw(ren);
                }
            }

            // رسم خط موقت (اگر وجود دارد)
            if (isActive && tempStart && tempEnd) {
                SDL_SetRenderDrawColor(ren, 255, 0, 0, 150); // رنگ قرمز برای خط موقت
                SDL_RenderDrawLine(ren,
                                   tempStart->x, tempStart->y,
                                   tempEnd->x, tempEnd->y);
            }
        }

        void clear() {
            for (auto &line: Lines) {
                //delete line;
            }
            Lines.clear();
            tempStart = nullptr;
            tempEnd = nullptr;
        }
    };

    vector<shared_ptr<Node>> Wire::allNodes;
    vector<shared_ptr<liine>> Wire::Lines;
    // کلاس جدید برای المان‌های مداری
    class Element {
    private:
        shared_ptr<Node> nodeN;
        shared_ptr<Node> nodeP;
        double voltage=0;
        double current=0;
    public:
        SDL_Rect rect;
        string name;
        string value;
        string type;
        SDL_Color color{0, 0, 255, 255};
        complex<double> AcValue=0;
        complex<double> AcVoltage=0;
        complex<double> AcCurrent=0;
        double phase=0;
        double fr=0;
        bool mirror=false;
        //Node *connectedNode; // اضافه کردن این خط
        //----------------------------------------------
        template <class Archive>
        void serialize(Archive& ar) {
            ar(nodeN,
               nodeP,
               voltage,
               current,
               rect.x,
               rect.y,
               rect.w,
               rect.h,
               name,
               value,
               type,
               color.r,
               color.g,
               color.b,
               color.a,
               AcValue,
               AcVoltage,
               AcCurrent,phase,fr,mirror);
        }
        Element(int x, int y,string type = "", string name = "", shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : type(type), name(name), nodeN(n), nodeP(p) {
            // Snap to grid during construction
            snapToGrid(x, y);
            ++x,++y;
            //connectedNode = new node(x, y); // ذخیره نود متصل
            rect = {x - 30, y - 15, stepX*4, stepY*2};// مرکز المان روی نود باشد
            mirror= false;
        }
        Element(){}
        virtual ~Element() = default;
        virtual void draw(SDL_Renderer* renderer, TTF_Font* font) =0;
        void Mirror(){
            mirror= !mirror;
            swap(nodeN,nodeP);
        }
        //phase 1
        virtual double getValue() const = 0;
        virtual void setValue(double val) = 0;
        string getType() const { return type; }
        string getName() const { return name; }
        shared_ptr<Node> getNodeN() const { return nodeN; }
        shared_ptr<Node> getNodeP() const { return nodeP; }
        double getVoltage(){
            return nodeP->getVoltage() - nodeN->getVoltage();
        }
        double getCurrent(){return current;}
        void setType(const string& t) { type = t; }
        void setName(const string& n) { name = n; }
        void setNodeN(shared_ptr<Node> n) { nodeN = n; }
        void setNodeP(shared_ptr<Node> p) { nodeP = p; }
        void setVoltage(double voltage){ this->voltage= voltage;}
        void setCurrent(double current){ this->current= current;}

    };

    class Resistor : public Element {
    private:
        double resistance;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<Element>(this), // سریالایز بخش پایه
               resistance);
        }
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (قهوه‌ای برای مقاومت)
            SDL_SetRenderDrawColor(renderer, 165, 42, 42, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // رسم گره‌ها
            SDL_Rect nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            SDL_Rect nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // متن
            string displayText = name + ": " + value;
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, displayText.c_str(), textColor);
            if(surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if(tex) {
                    SDL_Rect textRect{
                            rect.x + (rect.w - surf->w)/2,
                            rect.y + (rect.h - surf->h)/2,
                            surf->w,
                            surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &textRect);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
        Resistor(){}
        //phase 1
        double getValue() const override { return resistance; }
        Resistor(int x, int y,string name = "", double r = 0.0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"Resistor", name, n, p), resistance(r) {}
        void setValue(double r) override { resistance = r; }
        double getResistance() const { return resistance; }
        void setResistance(double r) { resistance = r; }
    };

    class Capacitor : public Element {
    private:
        double capacitance;
        double previousVoltage=0;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<Element>(this), // سریالایز بخش پایه
               capacitance,previousVoltage);
        }
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (قهوه‌ای برای مقاومت)
            SDL_SetRenderDrawColor(renderer, 165, 42, 42, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // رسم گره‌ها
            SDL_Rect nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            SDL_Rect nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // متن
            string displayText = name + ": " + value;
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, displayText.c_str(), textColor);
            if(surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if(tex) {
                    SDL_Rect textRect{
                            rect.x + (rect.w - surf->w)/2,
                            rect.y + (rect.h - surf->h)/2,
                            surf->w,
                            surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &textRect);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
        Capacitor(){}
        //phase 1
        Capacitor(int x, int y,string name = "", double c = 0.0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"Capacitor", name, n, p), capacitance(c) {
            previousVoltage=0;
        }
        double getValue() const override { return capacitance; }
        void setValue(double c) override { capacitance = c; }

        double getCapacitance() const { return capacitance; }
        void setCapacitance(double c) { capacitance = c; }
        double getPreviousVoltage(){
            return previousVoltage;
        }
        void setPreviousVoltage (double d){
            previousVoltage=d;
        }
    };

    class Inductor : public Element {
    private:
        double inductance;
        double previousCurrent=0.0;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<Element>(this), // سریالایز بخش پایه
               inductance,previousCurrent);
        }
        Inductor(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (قهوه‌ای برای مقاومت)
            SDL_SetRenderDrawColor(renderer, 165, 42, 42, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // رسم گره‌ها
            SDL_Rect nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            SDL_Rect nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // متن
            string displayText = name + ": " + value;
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, displayText.c_str(), textColor);
            if(surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if(tex) {
                    SDL_Rect textRect{
                            rect.x + (rect.w - surf->w)/2,
                            rect.y + (rect.h - surf->h)/2,
                            surf->w,
                            surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &textRect);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
        //phase 1
        Inductor(int x, int y,string name = "" ,double l = 0.0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"Inductor", name, n, p), inductance(l) {
            previousCurrent=0.0;
        }
        double getValue() const override { return inductance; }
        void setValue(double l) override { inductance = l; }
        double getInductance() const { return inductance; }
        void setInductance(double l) { inductance = l; }
        double getPreviousCurrent (){
            return previousCurrent;
        }
        void setPreviousCurrent (double d){
            previousCurrent=d;
        }
    };

    class VoltageSource : public Element{
    private:
        double Voltage;
    public:
        bool isAcSource=false;
        bool isPhaseSource=false;
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<Element>(this), // سریالایز بخش پایه
               Voltage,isAcSource,isPhaseSource);
        }
        VoltageSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        VoltageSource(int x, int y,string name = "", double c = 0.0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"VoltageSource", name, n, p), Voltage(c) {}
        double getValue() const override { return Voltage; }
        void setValue(double c) override { Voltage = c; }
        virtual void setValueAtTime(double t){
        }
    };

    class CurrentSource : public Element {
    private:
        double Current;

    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<Element>(this), // سریالایز بخش پایه
               Current);
        }
        CurrentSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {0, 0, 0, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        CurrentSource(int x, int y,string name = "", double c = 0.0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"CurrentSource", name, n, p), Current(c) {}
        virtual void setValueAtTime(double t){
        }
        double getValue() const override { return Current; }
        void setValue(double c) override { Current = c; }
    };

    class PWLVoltageSource : public VoltageSource {

    private:

        vector<pair<double, double>> timeVoltagePairs; // جفت‌های زمان-ولتاژ

        string pwlFileName; // نام فایل PWL
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<VoltageSource>(this),
               (timeVoltagePairs),
               (pwlFileName)
            );
        }
        PWLVoltageSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {

            // رنگ مستطیل (سبز برای منابع ولتاژ)

            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);

            SDL_RenderFillRect(renderer, &rect);

            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);

            SDL_RenderDrawRect(renderer, &rect);



            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن

            string nameText = name;

            SDL_Color textColor = {30, 30, 30, 255};

            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);

            if(nameSurf) {

                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);

                if(nameTex) {

                    SDL_Rect nameRect{

                            rect.x + (rect.w - nameSurf->w)/2,

                            rect.y - nameSurf->h - 5,

                            nameSurf->w,

                            nameSurf->h

                    };

                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);

                    SDL_DestroyTexture(nameTex);

                }

                SDL_FreeSurface(nameSurf);

            }



            // مقدار

            string valueText = "PWL (" + pwlFileName + ")";

            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);

            if(valueSurf) {

                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);

                if(valueTex) {

                    SDL_Rect valueRect{

                            rect.x + (rect.w - valueSurf->w)/2,

                            rect.y + rect.h + 5,

                            valueSurf->w,

                            valueSurf->h

                    };

                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);

                    SDL_DestroyTexture(valueTex);

                }

                SDL_FreeSurface(valueSurf);

            }

        }
        // سازنده

        PWLVoltageSource(int x, int y, string name = "",

                         string pwlFile = "data/PWL.txt",

                         shared_ptr<Node> n = nullptr,

                         shared_ptr<Node> p = nullptr)

                : VoltageSource(x, y, name, 0.0, n, p),

                  pwlFileName(pwlFile) {

            loadPWLFile(pwlFile);

        }



        // بارگذاری فایل PWL

        void loadPWLFile(const string& filename) {

            timeVoltagePairs.clear();

            ifstream file(filename);



            if (!file.is_open()) {

                cerr << "Error: Could not open PWL file " << filename << endl;

                return;

            }



            double time, voltage;

            while (file >> time >> voltage) {

                timeVoltagePairs.emplace_back(time, voltage);

            }



            file.close();



            // اگر داده‌ای وجود داشت، مقدار اولیه را تنظیم کنید

            if (!timeVoltagePairs.empty()) {

                setValue(timeVoltagePairs[0].second);

            }

        }



        // محاسبه مقدار ولتاژ در زمان t با استفاده از درونیابی خطی

        void setValueAtTime(double t) {

            if (timeVoltagePairs.empty()) {

                setValue(0.0);

                return;

            }



            // اگر زمان قبل از اولین نقطه باشد

            if (t <= timeVoltagePairs[0].first) {

                setValue(timeVoltagePairs[0].second);

                return;

            }



            // اگر زمان بعد از آخرین نقطه باشد

            if (t >= timeVoltagePairs.back().first) {

                setValue(timeVoltagePairs.back().second);

                return;

            }



            // پیدا کردن بازه مناسب

            for (size_t i = 1; i < timeVoltagePairs.size(); ++i) {

                if (t <= timeVoltagePairs[i].first) {

                    // درونیابی خطی

                    double t1 = timeVoltagePairs[i-1].first;

                    double v1 = timeVoltagePairs[i-1].second;

                    double t2 = timeVoltagePairs[i].first;

                    double v2 = timeVoltagePairs[i].second;



                    double voltage = v1 + (v2 - v1) * (t - t1) / (t2 - t1);

                    setValue(voltage);

                    return;

                }

            }

        }



        // توابع دسترسی

        const vector<pair<double, double>>& getTimeVoltagePairs() const { return timeVoltagePairs; }

        const string& getPwlFileName() const { return pwlFileName; }



        // توابع تنظیم

        void setPwlFile(const string& filename) {

            pwlFileName = filename;

            loadPWLFile(filename);

        }

    };

    class SinVoltageSource : public VoltageSource {
    private:
        double Voffset;
        double Vamplitude;
        double Frequency;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<VoltageSource>(this), // سریالایز بخش VoltageSource
               (Voffset),
               (Vamplitude),
               (Frequency)
            );
        }
        SinVoltageSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            ostringstream v;
            v<<"Sine(f: "<<Frequency<<",Amp: "<<Vamplitude<<",off: "<<Voffset<<")";
            string valueText = v.str();
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        SinVoltageSource(int x,int y,string name = "",
                         double offset = 0.0,
                         double amplitude = 0.0,
                         double freq = 0.0,
                         shared_ptr<Node> n = nullptr,
                         shared_ptr<Node> p = nullptr
        )
                : VoltageSource(x,y,name, 0.0,n,p),
                  Voffset(offset), Vamplitude(amplitude), Frequency(freq) {}
        double getOffset() const { return Voffset; }
        double getAmplitude() const { return Vamplitude; }
        double getFrequency() const { return Frequency; }
        void setValueAtTime(double t){
            setValue(Voffset+Vamplitude*sin(2*M_PI*Frequency*t));
        }
        void setOffset(double offset) { Voffset = offset; }
        void setAmplitude(double amplitude) { Vamplitude = amplitude; }
        void setFrequency(double freq) { Frequency = freq; }
    };

    class SinCurrentSource : public CurrentSource {
    private:
        double Voffset;
        double Vamplitude;
        double Frequency;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::base_class<CurrentSource>(this), // سریالایز بخش VoltageSource
               (Voffset),
               (Vamplitude),
               (Frequency)
            );
        }
        SinCurrentSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {0, 0, 0, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        SinCurrentSource(
                int x,int y,
                string name = "",
                double offset = 0.0,
                double amplitude = 0.0,
                double freq = 0.0,
                shared_ptr<Node> n = nullptr,
                shared_ptr<Node> p = nullptr
        )
                : CurrentSource(x,y,name, 0.0, n, p),
                  Voffset(offset), Vamplitude(amplitude), Frequency(freq) {}
        double getOffset() const { return Voffset; }
        double getAmplitude() const { return Vamplitude; }
        double getFrequency() const { return Frequency; }
        void setValueAtTime(double t){
            setValue(Voffset+Vamplitude*sin(2*M_PI*Frequency*t));
        }
        void setOffset(double offset) { Voffset = offset; }
        void setAmplitude(double amplitude) { Vamplitude = amplitude; }
        void setFrequency(double freq) { Frequency = freq; }
    };

    class PulseVoltageSource : public VoltageSource {
    private:
        double Vinitial;
        double Von;
        double Tdelay;
        double Trise;
        double Tfall;
        double Ton;
        double Tperiod;
        double Ncycles;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<VoltageSource>(this), // سریالایز بخش VoltageSource
                    Vinitial,
                    Von,
                    Tdelay,
                    Trise,
                    Tfall,
                    Ton,
                    Tperiod,
                    Ncycles
            );
        }
        PulseVoltageSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

// تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        PulseVoltageSource(
                int x,int y,
                const string& name = "",
                double Vinitial = 0.0,
                double Von = 0.0,
                double Tdelay = 0.0,
                double Trise = 0.0,
                double Tfall = 0.0,
                double Ton = 0.0,
                double Tperiod = 0.0,
                double Ncycles = 0.0,
                shared_ptr<Node> n = nullptr,
                shared_ptr<Node> p = nullptr
        )
                : VoltageSource(x,y,name, 0.0, n, p),
                  Vinitial(Vinitial), Von(Von), Tdelay(Tdelay),
                  Trise(Trise), Tfall(Tfall), Ton(Ton),
                  Tperiod(Tperiod), Ncycles(Ncycles) {}
        double getVinitial() const { return Vinitial; }
        double getVon() const { return Von; }
        double getTdelay() const { return Tdelay; }
        double getTrise() const { return Trise; }
        double getTfall() const { return Tfall; }
        double getTon() const { return Ton; }
        double getTperiod() const { return Tperiod; }
        double getNcycles() const { return Ncycles; }
        void setValueAtTime(double t){
            if (t < Tdelay) setValue(Vinitial);
            double localTime = t - Tdelay;
            double cycleTime = fmod(localTime, Tperiod);
            if (Ncycles != 0 && localTime > Ncycles * Tperiod) {
                setValue(Vinitial);
            }
            if (cycleTime < Trise) {
                setValue(Vinitial + (Von - Vinitial) * (cycleTime / Trise));
            }
            else if (cycleTime < Trise + Ton) {
                setValue( Von);
            }
            else if (cycleTime < Trise + Ton + Tfall) {
                setValue( Von - (Von - Vinitial) * ((cycleTime - Trise - Ton) / Tfall));
            }
            else {
                setValue( Vinitial);
            }
        }
        void setVinitial(double v) { Vinitial = v; }
        void setVon(double v) { Von = v; }
        void setTdelay(double t) { Tdelay = t; }
        void setTrise(double t) { Trise = t; }
        void setTfall(double t) { Tfall = t; }
        void setTon(double t) { Ton = t; }
        void setTperiod(double t) { Tperiod = t; }
        void setNcycles(double n) { Ncycles = n; }
    };

    class PulseCurrentSource : public CurrentSource {
    private:
        double Iinitial;
        double Ion;
        double Tdelay;
        double Trise;
        double Tfall;
        double Ton;
        double Tperiod;
        double Ncycles;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<CurrentSource>(this), // سریالایز بخش VoltageSource
                    Iinitial,
                    Ion,
                    Tdelay,
                    Trise,
                    Tfall,
                    Ton,
                    Tperiod,
                    Ncycles
            );
        }
        PulseCurrentSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {0, 0, 0, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        PulseCurrentSource(
                int x,int y,
                const string& name = "",
                double Iinitial = 0.0,
                double Ion = 0.0,
                double Tdelay = 0.0,
                double Trise = 0.0,
                double Tfall = 0.0,
                double Ton = 0.0,
                double Tperiod = 0.0,
                double Ncycles = 0.0,
                shared_ptr<Node> n = nullptr,
                shared_ptr<Node> p = nullptr
        )
                : CurrentSource(x,y,name, 0.0, n, p),
                  Iinitial(Iinitial), Ion(Ion), Tdelay(Tdelay),
                  Trise(Trise), Tfall(Tfall), Ton(Ton),
                  Tperiod(Tperiod), Ncycles(Ncycles) {}
        double getIinitial() const { return Iinitial; }
        double getIon() const { return Ion; }
        double getTdelay() const { return Tdelay; }
        double getTrise() const { return Trise; }
        double getTfall() const { return Tfall; }
        double getTon() const { return Ton; }
        double getTperiod() const { return Tperiod; }
        double getNcycles() const { return Ncycles; }
        void setValueAtTime(double t)  {
            if (t < Tdelay) setValue(Iinitial);
            double localTime = t - Tdelay;
            double cycleTime = fmod(localTime, Tperiod);
            if (Ncycles != 0 && localTime > Ncycles * Tperiod) {
                setValue( Iinitial);
            }
            if (cycleTime < Trise) {
                setValue(Iinitial + (Ion - Iinitial) * (cycleTime / Trise));
            }
            else if (cycleTime < Trise + Ton) {
                setValue(Ion);
            }
            else if (cycleTime < Trise + Ton + Tfall) {
                setValue(Ion - (Ion - Iinitial) * ((cycleTime - Trise - Ton) / Tfall));
            }
            else {
                setValue(Iinitial);
            }
        }
        void setIinitial(double i) { Iinitial = i; }
        void setIon(double i) { Ion = i; }
        void setTdelay(double t) { Tdelay = t; }
        void setTrise(double t) { Trise = t; }
        void setTfall(double t) { Tfall = t; }
        void setTon(double t) { Ton = t; }
        void setTperiod(double t) { Tperiod = t; }
        void setNcycles(double n) { Ncycles = n; }
    };

    class deltaVoltageSource : public VoltageSource {
    private:
        double tPulse;
        double area;
        double step=0.001;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<VoltageSource>(this), // سریالایز بخش VoltageSource
                    tPulse,area,step
            );
        }
        deltaVoltageSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        deltaVoltageSource(int x,int y,const std::string& name,
                           double tPulse, double step, double area = 1.0, shared_ptr<Node> n= nullptr, shared_ptr<Node> p= nullptr)
                : VoltageSource(x,y,name,0.0, n, p), tPulse(tPulse), area(area) {}
        void setValueAtTime(double t) override {
            if (std::fabs(t - tPulse) < 1e-12) {
                setVoltage( area / step);
            } else {
                setVoltage( 0.0);
            }
        }
        void setStep(double ste){
            step=ste;
        }
    };

    class deltaCurrentSource : public CurrentSource {
    private:
        double tPulse;
        double area;
        double step;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<CurrentSource>(this), // سریالایز بخش VoltageSource
                    tPulse,area,step
            );
        }
        deltaCurrentSource(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        deltaCurrentSource(int x,int y,const std::string& name,
                           double tPulse, double step, double area = 1.0, shared_ptr<Node> n= nullptr, shared_ptr<Node> p= nullptr)
                : CurrentSource(x,y,name, 0.0, n, p), tPulse(tPulse), area(area) {}
        void setValueAtTime(double t) override {
            if (std::fabs(t - tPulse) < 1e-12) {
                setCurrent(area / step);
            } else {
                setCurrent(0.0);
            }
        }
        void setStep(double ste){
            step=ste;
        }
    };

    class Diode : public Element {
    private:
        string model;
        double vOn;
        bool isOn;
        bool assumedOn=false;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<Element>(this), // سریالایز بخش VoltageSource
                    model,vOn,isOn,assumedOn
            );
        }
        Diode(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (نارنجی برای دیود)
            SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت (آند) سمت راست، منفی (کاتد) سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت (آند) سمت چپ، منفی (کاتد) سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم شکل دیود بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: مثلث به سمت راست (جهت جریان از آند به کاتد)
                // مثلث دیود (راست)
                SDL_Point diodeShape[4] = {
                        {centerX - 15, centerY - 10},
                        {centerX + 15, centerY},
                        {centerX - 15, centerY + 10},
                        {centerX - 15, centerY - 10}
                };
                SDL_RenderDrawLines(renderer, diodeShape, 4);

                // خط عمودی (کاتد)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 10, centerX + 15, centerY + 10);
            }
            else {
                // حالت چرخش: مثلث به سمت چپ (جهت جریان از آند به کاتد)
                // مثلث دیود (چپ)
                SDL_Point diodeShape[4] = {
                        {centerX + 15, centerY - 10},
                        {centerX - 15, centerY},
                        {centerX + 15, centerY + 10},
                        {centerX + 15, centerY - 10}
                };
                SDL_RenderDrawLines(renderer, diodeShape, 4);

                // خط عمودی (کاتد)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 10, centerX - 15, centerY + 10);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // نوع
            string typeText = type;
            SDL_Surface* typeSurf = TTF_RenderUTF8_Blended(font, typeText.c_str(), textColor);
            if(typeSurf) {
                SDL_Texture* typeTex = SDL_CreateTextureFromSurface(renderer, typeSurf);
                if(typeTex) {
                    SDL_Rect typeRect{
                            rect.x + (rect.w - typeSurf->w)/2,
                            rect.y + rect.h + 5,
                            typeSurf->w,
                            typeSurf->h
                    };
                    SDL_RenderCopy(renderer, typeTex, nullptr, &typeRect);
                    SDL_DestroyTexture(typeTex);
                }
                SDL_FreeSurface(typeSurf);
            }
        }
        //phase 1
        double getValue() const override { return vOn; }
        void setValue(double r) override {}
        Diode(int x, int y,string name = "", string mode = "", double v = 0, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : Element(x,y,"Diode" + mode, name, n, p), vOn(v), isOn(false) {}
        void updateState() {
            double v = getVoltage();
            isOn = (v >= vOn);
        }
        bool getIsOn() const { return isOn; }
        void assumeState(bool on) {
            this->assumedOn = on;
        }
        bool isAssumedOn() const { return assumedOn; }
    };

    class CCCS : public CurrentSource {
    private:
        double gain;
        shared_ptr<Element> element;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<Element>(this), // سریالایز بخش VoltageSource
                    gain,element
            );
        }
        CCCS(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {0, 0, 0, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        CCCS(int x, int y,std::string name = "", double gain = 1.0, shared_ptr<Element> e = nullptr, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : CurrentSource(x,y,name, 0.0, n, p), gain(gain), element(e) {
            setType("CCCS");
        }
        double getGain (){
            return gain;
        }
        shared_ptr<Element> getControlSourceName(){
            return element;
        }
        void setValueAtTime(double t=0){
            CurrentSource::setValue(gain*element->getCurrent());
        }
    };

    class CCVS : public VoltageSource {
    private:
        double gain;
        shared_ptr<Element> element;

    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<Element>(this), // سریالایز بخش VoltageSource
                    gain,element
            );
        }
        CCVS(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

// تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        CCVS(int x, int y,std::string name = "", double gain = 1.0, shared_ptr<Element> e = nullptr, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : VoltageSource(x,y,name, 0.0,n,p), gain(gain), element(e) {
            setType("CCVS");
        }
        double getGain() const {
            return gain;
        }
        shared_ptr<Element> getControlSourceName() const {
            return element;
        }
        void setValueAtTime(double t=0){
            if (element != nullptr) {
                setValue(gain * element->getCurrent());
            }
        }
    };

    class VCCS : public CurrentSource {
    private:
        double gain;
        shared_ptr<Node> ctrlPositive;
        shared_ptr<Node> ctrlNegative;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<Element>(this), // سریالایز بخش VoltageSource
                    gain,ctrlNegative,ctrlPositive
            );
        }
        VCCS(){}
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (بنفش برای منابع جریان)
            SDL_SetRenderDrawColor(renderer, 147, 112, 219, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم فلش بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: فلش از راست به چپ
                // بدنه فلش (از راست به چپ)
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX - 15, centerY);

                // سر فلش (سمت چپ)
                SDL_RenderDrawLine(renderer, centerX - 10, centerY - 5, centerX - 15, centerY);
                SDL_RenderDrawLine(renderer, centerX - 10, centerY + 5, centerX - 15, centerY);
            }
            else {
                // حالت چرخش: فلش از چپ به راست
                // بدنه فلش (از چپ به راست)
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX + 15, centerY);

                // سر فلش (سمت راست)
                SDL_RenderDrawLine(renderer, centerX + 10, centerY - 5, centerX + 15, centerY);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY + 5, centerX + 15, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {0, 0, 0, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        VCCS(int x, int y,std::string name = "",
             double gain = 1.0, shared_ptr<Node> ctrlPos = nullptr, shared_ptr<Node> ctrlNeg = nullptr, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : CurrentSource(x,y,name, 0.0, n, p), gain(gain), ctrlPositive(ctrlPos), ctrlNegative(ctrlNeg) {
            setType("VCCS");
        }
        double getGain() const {
            return gain;
        }
        void setValueAtTime(double t=0){
            if (ctrlPositive && ctrlNegative) {
                double vctrl = ctrlPositive->getVoltage() - ctrlNegative->getVoltage();
                setValue(gain * vctrl);
            }
        }
        shared_ptr<Node> getControlNodeP() const { return ctrlPositive; }
        shared_ptr<Node> getControlNodeN() const { return ctrlNegative; }
    };

    class VCVS : public VoltageSource {
    private:
        double gain;
        shared_ptr<Node> ctrlPositive;
        shared_ptr<Node> ctrlNegative;
    public:
        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                    cereal::base_class<Element>(this), // سریالایز بخش VoltageSource
                    gain,ctrlNegative,ctrlPositive
            );
        }
        VCVS(){};
        void draw(SDL_Renderer* renderer, TTF_Font* font) override {
            // رنگ مستطیل (سبز برای منابع ولتاژ)
            SDL_SetRenderDrawColor(renderer, 144, 238, 144, 255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // تعیین موقعیت گره‌ها بر اساس چرخش
            SDL_Rect nodeP, nodeN;

            if (!mirror) {
                // حالت عادی: گره مثبت سمت راست، منفی سمت چپ
                nodeP = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
            }
            else {
                // حالت چرخش 180 درجه: گره مثبت سمت چپ، منفی سمت راست
                nodeP = {rect.x, rect.y + (rect.h / 2) - 5, 10, 10};
                nodeN = {rect.x + rect.w - 10, rect.y + (rect.h / 2) - 5, 10, 10};
            }

            // رسم گره‌ها
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeP);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &nodeN);

            // رسم علامت + و - بر اساس چرخش
            int centerX = rect.x + rect.w / 2;
            int centerY = rect.y + rect.h / 2;

            if (!mirror) {
                // حالت عادی: + سمت راست، - سمت چپ
                // رسم + در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY - 5, centerX + 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX + 10, centerY, centerX + 20, centerY);
                // رسم - در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY, centerX - 25, centerY);
            }
            else {
                // حالت چرخش: + سمت چپ، - سمت راست
                // رسم + در سمت چپ
                SDL_RenderDrawLine(renderer, centerX - 15, centerY - 5, centerX - 15, centerY + 5);
                SDL_RenderDrawLine(renderer, centerX - 20, centerY, centerX - 10, centerY);
                // رسم - در سمت راست
                SDL_RenderDrawLine(renderer, centerX + 15, centerY, centerX + 25, centerY);
            }

            // متن
            string nameText = name;
            SDL_Color textColor = {30, 30, 30, 255};
            SDL_Surface* nameSurf = TTF_RenderUTF8_Blended(font, nameText.c_str(), textColor);
            if(nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if(nameTex) {
                    SDL_Rect nameRect{
                            rect.x + (rect.w - nameSurf->w)/2,
                            rect.y - nameSurf->h - 5,
                            nameSurf->w,
                            nameSurf->h
                    };
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nameRect);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // مقدار
            string valueText = value;
            SDL_Surface* valueSurf = TTF_RenderUTF8_Blended(font, valueText.c_str(), textColor);
            if(valueSurf) {
                SDL_Texture* valueTex = SDL_CreateTextureFromSurface(renderer, valueSurf);
                if(valueTex) {
                    SDL_Rect valueRect{
                            rect.x + (rect.w - valueSurf->w)/2,
                            rect.y + rect.h + 5,
                            valueSurf->w,
                            valueSurf->h
                    };
                    SDL_RenderCopy(renderer, valueTex, nullptr, &valueRect);
                    SDL_DestroyTexture(valueTex);
                }
                SDL_FreeSurface(valueSurf);
            }
        }
        //phase 1
        VCVS(int x, int y,std::string name = "",
             double gain = 1.0, shared_ptr<Node> ctrlPos = nullptr, shared_ptr<Node> ctrlNeg = nullptr, shared_ptr<Node> n = nullptr, shared_ptr<Node> p = nullptr)
                : VoltageSource(x,y,name, 0.0, n, p), gain(gain), ctrlPositive(ctrlPos), ctrlNegative(ctrlNeg) {
            setType("VCVS");
        }
        double getGain() const {
            return gain;
        }
        void setValueAtTime(double t=0){
            if (ctrlPositive && ctrlNegative) {
                double vctrl = ctrlPositive->getVoltage() - ctrlNegative->getVoltage();
                setValue(gain * vctrl);
            }
        }
        shared_ptr<Node> getControlNodeP() const { return ctrlPositive; }
        shared_ptr<Node> getControlNodeN() const { return ctrlNegative; }
    };
}
using namespace Circuit;
// ثبت انواع (Types)
CEREAL_REGISTER_TYPE(Resistor)
CEREAL_REGISTER_TYPE(Capacitor)
CEREAL_REGISTER_TYPE(Inductor)
CEREAL_REGISTER_TYPE(VoltageSource)
CEREAL_REGISTER_TYPE(CurrentSource)
CEREAL_REGISTER_TYPE(SinVoltageSource)
CEREAL_REGISTER_TYPE(SinCurrentSource)
CEREAL_REGISTER_TYPE(PWLVoltageSource)
CEREAL_REGISTER_TYPE(PulseVoltageSource)
CEREAL_REGISTER_TYPE(PulseCurrentSource)
CEREAL_REGISTER_TYPE(deltaVoltageSource)
CEREAL_REGISTER_TYPE(deltaCurrentSource)
CEREAL_REGISTER_TYPE(Diode)
CEREAL_REGISTER_TYPE(CCCS)
CEREAL_REGISTER_TYPE(CCVS)
CEREAL_REGISTER_TYPE(VCCS)
CEREAL_REGISTER_TYPE(VCVS)
// ثبت روابط ارث‌بری (Polymorphic Relations)
// روابط مستقیم با Element
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Resistor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Capacitor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Inductor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, VoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, CurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Element, Diode)

// روابط مشتق شده از VoltageSource
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, SinVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, PulseVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, deltaVoltageSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, CCVS)
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, VCVS)
CEREAL_REGISTER_POLYMORPHIC_RELATION(VoltageSource, PWLVoltageSource)
// روابط مشتق شده از CurrentSource
CEREAL_REGISTER_POLYMORPHIC_RELATION(CurrentSource, SinCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(CurrentSource, PulseCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(CurrentSource, deltaCurrentSource)
CEREAL_REGISTER_POLYMORPHIC_RELATION(CurrentSource, CCCS)
CEREAL_REGISTER_POLYMORPHIC_RELATION(CurrentSource, VCCS)

//////---------------------------------------
namespace graphicElements{
    class Probe {
    public:
        enum ProbeType { VOLTAGE, CURRENT, POWER };
        ProbeType type;
        string target; // می‌تواند نام نود یا نام المان باشد
        string expression; // عبارت کامل مانند V(N1) یا I(R1)

        Probe(ProbeType t, const string& tg) : type(t), target(tg) {
            updateExpression();
        }

        Probe(){};

        template <class Archive>
        void serialize(Archive& ar) {
            ar(type, target, expression);
        }

        void updateExpression() {
            if (type == VOLTAGE) {
                expression = "V(" + target + ")";
            } else if (type == CURRENT) {
                expression = "I(" + target + ")";
            } else if (type == POWER) {
                expression = "P(" + target + ")";
            }
        }
    };

    struct DataPoint {
        std::string signalName;
        double x;
        double y;
    };

    class DataVisualizer {
    private:
        SDL_Window* window;
        SDL_Renderer* renderer;
        TTF_Font* font;
        std::vector<DataPoint> dataPoints;
        std::map<std::string, SDL_Color> signalColors;
        std::vector<std::string> signalNames;
        SDL_Color textColor = {0, 0, 0, 255};

        bool showColorMenu = false;
        std::string selectedSignal;
        SDL_Rect colorMenuRect;
        std::vector<SDL_Rect> colorRects;

        // Cursor-related members
        struct Cursor {
            bool active = false;
            double x = 0.0;
            double y = 0.0;
            SDL_Color color;
            std::string name;
            std::string attachedSignal; // Signal name this cursor is attached to
        };
        Cursor cursorA, cursorB;
        bool cursorDragging = false;
        Cursor* activeCursor = nullptr;

        const double vdivs[37] = {
                1e-6,  2e-6,  5e-6,
                1e-5,  2e-5,  5e-5,
                1e-4,  2e-4,  5e-4,
                1e-3,  2e-3,  5e-3,
                1e-2,  2e-2,  5e-2,
                1e-1,  2e-1,  5e-1,
                1,     2,     5,
                10,    20,    50,
                100,   200,   500,
                1000,  2000,  5000,
                10000, 20000, 50000,
                100000,200000,500000,
                1000000
        };
        const double tdivs[33] = {
                1e-9, 2e-9, 5e-9,
                1e-8, 2e-8, 5e-8,
                1e-7, 2e-7, 5e-7,
                1e-6, 2e-6, 5e-6,
                1e-5, 2e-5, 5e-5,
                1e-4, 2e-4, 5e-4,
                1e-3, 2e-3, 5e-3,
                1e-2, 2e-2, 5e-2,
                0.1, 0.2, 0.5,
                1.0, 2.0, 5.0,
                10.0, 20.0, 50.0
        };
        int vdivIndex = 18;
        int tdivIndex = 26;

        double offsetX = 0.0;
        double offsetY = 0.0;
        bool autoZoom = true;

        double minX = 0, maxX = 0;
        double minY = 0, maxY = 0;
        int gridSize = 70;

        double timePerDivision = tdivs[tdivIndex], valuePerDivision = vdivs[vdivIndex];

        // رنگ‌های پیش‌فرض برای سیگنال‌ها
        const std::vector<SDL_Color> defaultColors = {
                {255, 0, 0, 255},    // قرمز
                {0, 0, 255, 255},     // آبی
                {0, 128, 0, 255},     // سبز
                {128, 0, 128, 255},   // بنفش
                {255, 165, 0, 255},   // نارنجی
                {0, 255, 255, 255},   // فیروزه‌ای
                {255, 0, 255, 255},   // صورتی
                {128, 128, 128, 255}, // خاکستری
                {0, 0, 0, 255},       // سیاه
                {255, 255, 0, 255}    // زرد
        };

    public:
        DataVisualizer(const std::string& filename) {
            // Initialize cursors
            cursorA.color = {50, 50,50, 255}; // Red
            cursorA.name = "A";
            cursorB.color = {50, 50, 50, 255}; // Blue
            cursorB.name = "B";

            if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
                return;
            }

            if (TTF_Init() == -1) {
                std::cerr << "TTF initialization failed: " << TTF_GetError() << std::endl;
                SDL_Quit();
                return;
            }

            window = SDL_CreateWindow("Data Visualizer",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      12 * gridSize, 10 * gridSize,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

            if (!window) {
                std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
                TTF_Quit();
                SDL_Quit();
                return;
            }

            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (!renderer) {
                std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
                SDL_DestroyWindow(window);
                TTF_Quit();
                SDL_Quit();
                return;
            }

            font = TTF_OpenFont("assets/tahoma.ttf", 16);
            if (!font) {
                std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
            }

            loadDataFromFile(filename);
            assignColorsToSignals();
            calculateAutoZoom();
        }

        ~DataVisualizer() {
            if (font) TTF_CloseFont(font);
            if (renderer) SDL_DestroyRenderer(renderer);
            if (window) SDL_DestroyWindow(window);
        }

        void assignColorsToSignals() {
            for (size_t i = 0; i < signalNames.size(); ++i) {
                signalColors[signalNames[i]] = defaultColors[i % defaultColors.size()];
            }
        }

        bool parseDataPoint(const std::string& line, double& outX, double& outY) {
            std::istringstream iss(line);
            std::string xStr, yStr;

            // 1. سعی کن رشته را بر اساس کاما به دو بخش تقسیم کنی
            if (std::getline(iss, xStr, ',') && std::getline(iss, yStr)) {
                // 2. بررسی کن که آیا کاراکتر اضافی بعد از بخش دوم وجود دارد یا خیر
                if (iss.rdbuf()->in_avail() != 0) {
                    return false; // مثال: "1.2,3.4,5.6" یک نقطه داده معتبر نیست
                }

                try {
                    // 3. سعی کن هر دو بخش را به عدد (double) تبدیل کنی
                    outX = std::stod(xStr);
                    outY = std::stod(yStr);
                    return true; // اگر هر دو موفقیت‌آمیز بود، این یک نقطه داده است
                } catch (const std::invalid_argument& ia) {
                    // اگر تبدیل ناموفق بود (مثلا "a,b")
                    return false;
                } catch (const std::out_of_range& oor) {
                    // اگر عدد خارج از محدوده بود
                    return false;
                }
            }
            // اگر ساختار "بخش۱,بخش۲" را نداشت
            return false;
        }

        void loadDataFromFile(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Failed to open file: " << filename << std::endl;
                return;
            }

            std::string currentSignal;
            std::string line;
            bool firstPoint = true;

            while (std::getline(file, line)) {
                // حذف فاصله‌های سفید از ابتدا و انتهای خط
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                if (line.empty()) continue;

                double x, y;
                // بررسی می‌کنیم آیا خط فعلی یک نقطه داده است یا خیر
                if (parseDataPoint(line, x, y)) {
                    // اگر بود، آن را پردازش می‌کنیم
                    if (currentSignal.empty()) {
                        std::cerr << "Warning: Data point found without a preceding signal name: " << line << std::endl;
                        continue;
                    }

                    dataPoints.push_back({currentSignal, x, y});

                    if (firstPoint) {
                        minX = maxX = x;
                        minY = maxY = y;
                        firstPoint = false;
                    } else {
                        minX = std::min(minX, x);
                        maxX = std::max(maxX, x);
                        minY = std::min(minY, y);
                        maxY = std::max(maxY, y);
                    }
                } else {
                    // اگر یک نقطه داده نبود، پس حتما نام یک سیگنال جدید است
                    currentSignal = line;

                    // اگر سیگنال جدید است، به لیست اضافه می‌کنیم
                    if (std::find(signalNames.begin(), signalNames.end(), currentSignal) == signalNames.end()) {
                        signalNames.push_back(currentSignal);
                    }
                }
            }
        }

        void calculateAutoZoom() {
            if (dataPoints.empty()) return;

            int w, h;
            SDL_GetWindowSize(window, &w, &h);

            double dataWidth = maxX - minX;
            double dataHeight = maxY - minY;

            if (dataWidth == 0) dataWidth = 1;
            if (dataHeight == 0) dataHeight = 1;

            timePerDivision = dataWidth / 12;
            valuePerDivision = dataHeight / 10;
            offsetX = (maxX + minX) / 2;
            offsetY = (maxY + minY) / 2;
        }

        void run() {
            if (!window || !renderer) return;

            bool running = true;
            SDL_Event event;

            while (running) {
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        running = false; // اینجا اختیاریه، اگر خواستید همه بسته بشن
                    }
                    else if (event.type == SDL_WINDOWEVENT &&
                             event.window.event == SDL_WINDOWEVENT_CLOSE &&
                             event.window.windowID == SDL_GetWindowID(window)) {
                        running = false; // فقط همین پنجره بسته شود
                    }
                    else if (event.type == SDL_KEYDOWN) {
                        handleKeyEvent(event.key);
                    }
                    else if (event.type == SDL_MOUSEBUTTONDOWN) {
                        handleMouseEvent(event.button);
                    } else if (event.type == SDL_MOUSEBUTTONUP) {
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            cursorDragging = false;
                            activeCursor = nullptr;
                        }
                    } else if (event.type == SDL_MOUSEMOTION) {
                        if (cursorDragging && activeCursor) {
                            int w, h;
                            SDL_GetWindowSize(window, &w, &h);

                            // Convert screen coordinates to data coordinates
                            activeCursor->x = offsetX + (event.motion.x - w/2) * timePerDivision / gridSize;

                            // If cursor is attached to a signal, find the corresponding Y value
                            if (!activeCursor->attachedSignal.empty()) {
                                activeCursor->y = getSignalYValue(activeCursor->attachedSignal, activeCursor->x);
                            } else {
                                activeCursor->y = offsetY + (h/2 - event.motion.y) * valuePerDivision / gridSize;
                            }
                        }
                    } else if (event.type == SDL_WINDOWEVENT &&
                               event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        if (autoZoom) {
                            calculateAutoZoom();
                        }
                    }
                }

                render();
                SDL_Delay(16);
            }
        }

    private:
        // Function to get Y value of a signal at specific X coordinate
        double getSignalYValue(const std::string& signalName, double x) {
            std::vector<DataPoint> signalPoints;
            for (const auto& point : dataPoints) {
                if (point.signalName == signalName) {
                    signalPoints.push_back(point);
                }
            }

            if (signalPoints.empty()) return 0.0;

            // Find the two points surrounding the x coordinate
            for (size_t i = 1; i < signalPoints.size(); i++) {
                if (signalPoints[i-1].x <= x && signalPoints[i].x >= x) {
                    // Linear interpolation
                    double x0 = signalPoints[i-1].x;
                    double y0 = signalPoints[i-1].y;
                    double x1 = signalPoints[i].x;
                    double y1 = signalPoints[i].y;

                    if (x1 == x0) return y0; // Avoid division by zero

                    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
                }
            }

            // If x is outside the range, return the first or last point
            if (x < signalPoints.front().x) return signalPoints.front().y;
            return signalPoints.back().y;
        }

        void handleKeyEvent(SDL_KeyboardEvent& key) {
            switch (key.keysym.sym) {
                case SDLK_LEFT:
                    offsetX -= timePerDivision / 2;
                    autoZoom = false;
                    break;
                case SDLK_RIGHT:
                    offsetX += timePerDivision / 2;
                    autoZoom = false;
                    break;
                case SDLK_UP:
                    offsetY += valuePerDivision / 2;
                    autoZoom = false;
                    break;
                case SDLK_DOWN:
                    offsetY -= valuePerDivision / 2;
                    autoZoom = false;
                    break;
                case SDLK_MINUS:
                    if (SDL_GetModState() & KMOD_SHIFT) {
                        if (tdivIndex < 32)
                            timePerDivision = tdivs[++tdivIndex];
                    } else {
                        if (vdivIndex < 35)
                            valuePerDivision = vdivs[++vdivIndex];
                    }
                    autoZoom = false;
                    break;
                case SDLK_EQUALS:
                    if (SDL_GetModState() & KMOD_SHIFT) {
                        if (tdivIndex > 0)
                            timePerDivision = tdivs[--tdivIndex];
                    } else {
                        if (vdivIndex > 0)
                            valuePerDivision = vdivs[--vdivIndex];
                    }
                    autoZoom = false;
                    break;
                case SDLK_a:
                    autoZoom = true;
                    calculateAutoZoom();
                    break;
                case SDLK_r:
                    offsetY = 0.0;
                    offsetX = 0.0;
                    autoZoom = false;
                    break;
                case SDLK_F1: // Toggle cursor A
                    cursorA.active = !cursorA.active;
                    if (cursorA.active && !cursorA.x && !cursorA.y) {
                        cursorA.x = offsetX;
                        cursorA.y = offsetY;
                    }
                    break;
                case SDLK_F2: // Toggle cursor B
                    cursorB.active = !cursorB.active;
                    if (cursorB.active && !cursorB.x && !cursorB.y) {
                        cursorB.x = offsetX;
                        cursorB.y = offsetY;
                    }
                    break;
                case SDLK_F3: // Attach cursor A to selected signal
                    if (!selectedSignal.empty()) {
                        cursorA.attachedSignal = selectedSignal;
                        cursorA.y = getSignalYValue(selectedSignal, cursorA.x);
                    }
                    break;
                case SDLK_F4: // Attach cursor B to selected signal
                    if (!selectedSignal.empty()) {
                        cursorB.attachedSignal = selectedSignal;
                        cursorB.y = getSignalYValue(selectedSignal, cursorB.x);
                    }
                    break;
                case SDLK_F5: // Detach cursor A from signal
                    cursorA.attachedSignal.clear();
                    break;
                case SDLK_F6: // Detach cursor B from signal
                    cursorB.attachedSignal.clear();
                    break;
                case SDLK_ESCAPE:
                    SDL_Event quitEvent;
                    quitEvent.type = SDL_WINDOWEVENT;
                    quitEvent.window.event = SDL_WINDOWEVENT_CLOSE;
                    quitEvent.window.windowID = SDL_GetWindowID(window); // بستن همین پنجره
                    SDL_PushEvent(&quitEvent);
                    break;
            }
        }

        void handleMouseEvent(SDL_MouseButtonEvent& mouse) {
            if (mouse.button == SDL_BUTTON_LEFT) {
                if (showColorMenu) {
                    changeSignalColor(mouse.x, mouse.y);
                } else if (isLegendClicked(mouse.x, mouse.y)) {
                    showColorMenu = true;
                } else {
                    // Check if clicked near a cursor to start dragging
                    int x, y;
                    dataToScreen(cursorA.x, cursorA.y, x, y);
                    if (cursorA.active && abs(mouse.x - x) < 10 && abs(mouse.y - y) < 50) {
                        cursorDragging = true;
                        activeCursor = &cursorA;
                        return;
                    }

                    dataToScreen(cursorB.x, cursorB.y, x, y);
                    if (cursorB.active && abs(mouse.x - x) < 10 && abs(mouse.y - y) < 50) {
                        cursorDragging = true;
                        activeCursor = &cursorB;
                        return;
                    }

                    // If not dragging a cursor, place the active cursor
                    if (activeCursor) {
                        int w, h;
                        SDL_GetWindowSize(window, &w, &h);
                        activeCursor->x = offsetX + (mouse.x - w/2) * timePerDivision / gridSize;

                        // If cursor is attached to a signal, find the corresponding Y value
                        if (!activeCursor->attachedSignal.empty()) {
                            activeCursor->y = getSignalYValue(activeCursor->attachedSignal, activeCursor->x);
                        } else {
                            activeCursor->y = offsetY + (h/2 - mouse.y) * valuePerDivision / gridSize;
                        }
                    }
                }
            } else if (mouse.button == SDL_BUTTON_RIGHT) {
                if (showColorMenu) {
                    showColorMenu = false;
                    selectedSignal.clear();
                } else {
                    // Right click toggles which cursor is active for placement
                    if (activeCursor == &cursorA) {
                        activeCursor = &cursorB;
                    } else {
                        activeCursor = &cursorA;
                    }
                }
            }
        }

        void dataToScreen(double dataX, double dataY, int& screenX, int& screenY) {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);

            screenX = static_cast<int>((dataX - offsetX) / timePerDivision * gridSize + w / 2);
            screenY = static_cast<int>(h / 2 - (dataY - offsetY) / valuePerDivision * gridSize);
        }

        void drawGrid() {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);

            SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);

            for (int i = 0; i <= w; i += gridSize) {
                SDL_RenderDrawLine(renderer, i, 0, i, h);
            }
            for (int i = 0; i <= h; i += gridSize) {
                SDL_RenderDrawLine(renderer, 0, i, w, i);
            }
        }

        void drawAxes() {
            int w,h; SDL_GetWindowSize(window,&w,&h);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(renderer, 0, h/2, w, h/2);
            SDL_RenderDrawLine(renderer, 0, h/2+1, w, h/2+1);
            SDL_RenderDrawLine(renderer, w/2+1, 0, w/2+1, h);
            SDL_RenderDrawLine(renderer, w/2, 0, w/2, h);
            if (font) {
                std::ostringstream info;
                info << "Vertical: " << valuePerDivision << " units/div\n"
                     << "Horizontal: " << timePerDivision << " units/div\n"
                     << "Pan: (" << offsetX << ", " << offsetY << ")\n"
                     << "AutoZoom: " << (autoZoom ? "ON" : "OFF") << "\n";
                renderText(info.str(), 10, 10);
            }
        }

        void renderText(const std::string& text, int x, int y) {
            if (!font) return;
            SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), textColor, 800);
            if (!surface) { std::cerr << "Failed to create surface: " << TTF_GetError() << std::endl; return; }
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (!texture) { SDL_FreeSurface(surface); return; }
            SDL_Rect rect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &rect);
            SDL_FreeSurface(surface); SDL_DestroyTexture(texture);
        }

        void drawLegend() {
            if (!font || signalNames.empty()) return;

            int startX = 10;
            int startY = 120;
            int boxSize = 20;
            int textOffset = 5;
            int lineHeight = 25;

            for (int i = 0; i < static_cast<int>(signalNames.size()); ++i) {
                const auto& name = signalNames[i];
                const auto& color = signalColors[name];

                SDL_Rect rect = {startX, startY + i * lineHeight, boxSize, boxSize};

                // رسم مستطیل با رنگ سیگنال
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &rect);

                // رسم حاشیه مشکی دور مستطیل
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &rect);

                // Highlight selected signal
                if (name == selectedSignal) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                    SDL_Rect highlightRect = {rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
                    SDL_RenderDrawRect(renderer, &highlightRect);
                }

                renderText(name, startX + boxSize + textOffset, startY + i * lineHeight);
            }
        }

        bool isLegendClicked(int x, int y) {
            int startX = 10;
            int startY = 120;
            int boxSize = 20;
            int lineHeight = 25;

            for (int i = 0; i < static_cast<int>(signalNames.size()); ++i) {
                SDL_Rect rect = {startX, startY + i * lineHeight, boxSize, boxSize};
                if (x >= rect.x && x <= rect.x + rect.w &&
                    y >= rect.y && y <= rect.y + rect.h) {
                    selectedSignal = signalNames[i];
                    return true;
                }
            }
            return false;
        }

        void showColorSelectionMenu() {
            if (selectedSignal.empty()) return;

            // تنظیم موقعیت و اندازه منوی رنگ
            colorMenuRect = {100, 100, 200, static_cast<int>(defaultColors.size()) * 30 + 20};

            // رسم پس‌زمینه منو
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
            SDL_RenderFillRect(renderer, &colorMenuRect);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &colorMenuRect);

            // رسم عنوان منو
            renderText("Select color for " + selectedSignal, colorMenuRect.x + 10, colorMenuRect.y + 5);

            // ذخیره مستطیل‌های رنگ‌ها برای تشخیص کلیک
            colorRects.clear();
            for (size_t i = 0; i < defaultColors.size(); ++i) {
                SDL_Rect rect = {
                        colorMenuRect.x + 10,
                        colorMenuRect.y + 30 + static_cast<int>(i) * 30,
                        180,
                        25
                };
                colorRects.push_back(rect);

                // رسم رنگ
                const auto& color = defaultColors[i];
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &rect);

                // رسم حاشیه
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &rect);
            }
        }

        void changeSignalColor(int x, int y) {
            for (size_t i = 0; i < colorRects.size(); ++i) {
                if (x >= colorRects[i].x && x <= colorRects[i].x + colorRects[i].w &&
                    y >= colorRects[i].y && y <= colorRects[i].y + colorRects[i].h) {
                    signalColors[selectedSignal] = defaultColors[i];
                    showColorMenu = false;
                    selectedSignal.clear();
                    break;
                }
            }
        }

        void drawCursor(const Cursor& cursor) {
            if (!cursor.active) return;

            int w, h;
            SDL_GetWindowSize(window, &w, &h);

            int x, y;
            dataToScreen(cursor.x, cursor.y, x, y);

            // Draw vertical line
            SDL_SetRenderDrawColor(renderer, cursor.color.r, cursor.color.g, cursor.color.b, 255);
            SDL_RenderDrawLine(renderer, x, 0, x, h);

            // Draw horizontal line
            SDL_RenderDrawLine(renderer, 0, y, w, y);

            // Draw cursor label
            std::ostringstream label;
            label << cursor.name;

            renderText(label.str(), x + 5, y + 5);
        }

        void drawCursorInfoBox() {
            if (!cursorA.active && !cursorB.active) return;

            int boxWidth = 350;
            int boxHeight = 120;
            int margin = 10;
            int w, h;
            SDL_GetWindowSize(window, &w, &h);

            SDL_Rect box = {margin, h-margin-boxHeight, boxWidth, boxHeight};

            // Draw box background
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 200);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &box);

            int yOffset = margin -5;
            if (cursorA.active) {
                std::ostringstream oss;
                oss << "Cursor A: (" << std::scientific << std::setprecision(4)
                    << cursorA.x << ", " << cursorA.y << ")";
                if (!cursorA.attachedSignal.empty()) {
                    oss << " [" << cursorA.attachedSignal << "]";
                }
                renderText(oss.str(), 2*margin, h-boxHeight+yOffset);
                yOffset += 20;
            }

            if (cursorB.active) {
                std::ostringstream oss;
                oss << "Cursor B: (" << std::scientific << std::setprecision(4)
                    << cursorB.x << ", " << cursorB.y << ")";
                if (!cursorB.attachedSignal.empty()) {
                    oss << " [" << cursorB.attachedSignal << "]";
                }
                renderText(oss.str(), 2*margin, h-boxHeight+yOffset);
                yOffset += 20;
            }

            if (cursorA.active && cursorB.active) {
                double deltaX = cursorB.x - cursorA.x;
                double deltaY = cursorB.y - cursorA.y;
                double slope = (deltaX != 0) ? deltaY / deltaX : 0;

                std::ostringstream oss;
                oss << "dX: " << std::scientific << std::setprecision(4) << deltaX;
                renderText(oss.str(), 2*margin, h-boxHeight+yOffset);
                yOffset += 20;

                oss.str("");
                oss << "dY: " << std::scientific << std::setprecision(4) << deltaY;
                renderText(oss.str(), 2*margin, h-boxHeight+yOffset);
                yOffset += 20;

                oss.str("");
                oss << "Slope: " << std::scientific << std::setprecision(4) << slope;
                renderText(oss.str(), 2*margin, h-boxHeight+yOffset);
            }
        }

        void render() {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);

            drawGrid();
            drawAxes();

            if (!dataPoints.empty()) {
                std::map<std::string, std::vector<DataPoint>> groupedPoints;
                for (const auto& point : dataPoints) {
                    groupedPoints[point.signalName].push_back(point);
                }

                for (const auto& entry : groupedPoints) {
                    const auto& signalName = entry.first;
                    const auto& points = entry.second;
                    const auto& color = signalColors[signalName];

                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

                    for (size_t i = 1; i < points.size(); ++i) {
                        int x1, y1, x2, y2;
                        dataToScreen(points[i-1].x, points[i-1].y, x1, y1);
                        dataToScreen(points[i].x, points[i].y, x2, y2);
                        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                    }
                }
            }

            // Draw cursors
            drawCursor(cursorA);
            drawCursor(cursorB);

            // Draw cursor info box
            drawCursorInfoBox();

            drawLegend();

            if (showColorMenu) {
                showColorSelectionMenu();
            }
            SDL_RenderPresent(renderer);
        }
    };

    // کلاس TextBox (همان کد قبلی)
    class TextBox {
    public:
        SDL_Rect rect;
        SDL_Color bgColor{255, 255, 255, 255};
        SDL_Color borderColor{0, 0, 0, 255};
        SDL_Color textColor{0, 0, 0, 255};
        bool active = false;
        std::string text;
        shared_ptr<TTF_Font> font = nullptr;
        std::string fontPath="assets/Tahoma.ttf";
        TextBox() = default;
        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز SDL_Rect (ذخیره به صورت چهار عدد صحیح)
            ar(rect.x, rect.y, rect.w, rect.h);

            // سریالایز SDL_Colorها (ذخیره به صورت چهار عدد صحیح برای هر کدام)
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(textColor.r, textColor.g, textColor.b, textColor.a);

            // سریالایز سایر اعضا
            ar(active, text, fontPath); // fontPath را ذخیره می‌کنیم به جای خود font
        }

        TextBox(int x, int y, int w, int h, shared_ptr<TTF_Font> f) {
            rect = {x, y, w, h};
            font = f;
        }

        void handleEvent(const SDL_Event& e) {
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                SDL_Point mouse{e.button.x, e.button.y};
                active = SDL_PointInRect(&mouse, &rect);
            }
            if (active && e.type == SDL_TEXTINPUT) {
                text += e.text.text;
            }
            if (active && e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE && !text.empty()) {
                text.pop_back();
            }
            if (active && e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                active = false;
            }
        }
        void draw(SDL_Renderer* renderer) {
            // پس‌زمینه
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);

            // حاشیه (اگر فعال است آبی)
            if (active) {
                SDL_SetRenderDrawColor(renderer, 0, 120, 215, 255); // آبی شبیه Windows
            } else {
                SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            }
            SDL_RenderDrawRect(renderer, &rect);

            // متن
            if (!text.empty() && font) {
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), textColor);
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect dst{rect.x + 5, rect.y + (rect.h - surf->h) / 2, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_FreeSurface(surf);
                SDL_DestroyTexture(tex);
            }
        }
    };
    // کلاس Button (همان کد قبلی)
    class Button {
    public:
        SDL_Rect rect;
        SDL_Color bgColor{ 100, 149, 237, 255 };     // Normal
        SDL_Color hoverColor{ 120, 170, 255, 255 };  // Hover
        SDL_Color pressedColor{ 70, 110, 200, 255 }; // Pressed
        SDL_Color disabledColor{ 180, 180, 180, 255 };
        SDL_Color borderColor{ 0, 0, 0, 255 };
        SDL_Color textColor{ 255, 255, 255, 255 };
        SDL_Keycode shortcutKey = SDLK_UNKNOWN;
        SDL_Keymod shortcutMod = KMOD_NONE;
        bool shortcutEnabled = false;
        int borderThickness = 1;
        bool enabled = true;
        bool hovered = false;
        bool pressed = false;
        bool clicked = false;
        std::string text;
        shared_ptr<TTF_Font> font = nullptr;
        std::function<void()> onClick = nullptr;
        std::string fontPath; // برای ذخیره مسیر فونت
        std::string callbackID; // شناسه برای تابع callback
        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز SDL_Rect
            ar(rect.x, rect.y, rect.w, rect.h);

            // سریالایز رنگ‌ها
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a);
            ar(pressedColor.r, pressedColor.g, pressedColor.b, pressedColor.a);
            ar(disabledColor.r, disabledColor.g, disabledColor.b, disabledColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(textColor.r, textColor.g, textColor.b, textColor.a);

            // سریالایز کلیدهای میانبر
            ar(shortcutKey, shortcutMod, shortcutEnabled);

            // سریالایز وضعیت دکمه
            ar(borderThickness, enabled, hovered, pressed, clicked);

            // سریالایز متن و فونت
            ar(text, fontPath, callbackID);
        }

        // ... سایر توابع کلاس
        Button() = default;
        Button(int x, int y, int w, int h, shared_ptr<TTF_Font> f, const std::string& t)
        {
            rect = { x, y, w, h };
            font = f;
            text = t;
        }
        void setShortcut(SDL_Keycode key, int mod = KMOD_NONE) {
            shortcutKey = key;
            shortcutMod = (SDL_Keymod)mod;
            shortcutEnabled = true;
        }
        void disableShortcut() {
            shortcutEnabled = false;
        }
        void handleEvent(const SDL_Event& e) {
            if (!enabled) return;

            // بررسی رویدادهای ماوس (همانند قبل)
            if (e.type == SDL_MOUSEMOTION) {
                SDL_Point p{ e.motion.x, e.motion.y };
                hovered = SDL_PointInRect(&p, &rect);
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Point p{ e.button.x, e.button.y };
                if (SDL_PointInRect(&p, &rect)) pressed = true;
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Point p{ e.button.x, e.button.y };
                bool inside = SDL_PointInRect(&p, &rect);
                bool wasPressed = pressed;
                pressed = false;
                if (inside && wasPressed) {
                    triggerClick();
                }
                hovered = inside;
            }

            // بررسی رویدادهای کیبورد برای کلیدهای میانبر
            if (shortcutEnabled && e.type == SDL_KEYDOWN) {
                // بررسی اینکه کلید فشار داده شده با کلید میانبر مطابقت دارد
                if (e.key.keysym.sym == shortcutKey) {
                    // بررسی مودهای کیبورد (مانند Ctrl, Alt, Shift)
                    SDL_Keymod currentMod = static_cast<SDL_Keymod>(e.key.keysym.mod);
                    if ((currentMod & shortcutMod) == shortcutMod) {
                        triggerClick();
                    }
                }
            }
        }
        void draw(SDL_Renderer* renderer) {
            SDL_Color bg = bgColor;
            if (!enabled) bg = disabledColor;
            else if (pressed) bg = pressedColor;
            else if (hovered) bg = hoverColor;

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(renderer, &rect);

            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            for (int i = 0; i < borderThickness; ++i) {
                SDL_Rect r{ rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i };
                SDL_RenderDrawRect(renderer, &r);
            }

            if (!text.empty() && font) {
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), textColor);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        int offsetY = pressed ? 1 : 0;
                        SDL_Rect dst{
                                rect.x + (rect.w - surf->w) / 2,
                                rect.y + (rect.h - surf->h) / 2 + offsetY,
                                surf->w, surf->h
                        };
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
        }
    private:
        void triggerClick() {
            if (onClick) onClick();
            else clicked = true;
        }
    };
    //کلاس نمایش متن
    class messageBox {
    public:
        SDL_Rect rect;
        SDL_Color bgColor{240, 240, 240, 255};
        SDL_Color borderColor{100, 100, 100, 255};
        SDL_Color textColor{0, 0, 0, 255};
        SDL_Color titleColor{200, 0, 0, 255}; // رنگ عنوان (قرمز برای خطا)
        std::string message;
        std::string title;
        shared_ptr<TTF_Font> font = nullptr;
        shared_ptr<TTF_Font> titleFont = nullptr;
        Button okButton;
        bool visible = false;
        std::string fontPath;       // مسیر فونت اصلی
        std::string titleFontPath;  // مسیر فونت عنوان
        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز SDL_Rect
            ar(rect.x, rect.y, rect.w, rect.h);

            // سریالایز رنگ‌ها
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(textColor.r, textColor.g, textColor.b, textColor.a);
            ar(titleColor.r, titleColor.g, titleColor.b, titleColor.a);

            // سریالایز متن‌ها
            ar(message, title);

            // سریالایز مسیر فونت‌ها
            ar(fontPath, titleFontPath);

            // سریالایز دکمه OK
            ar(okButton);

            // سریالایز وضعیت visibility
            ar(visible);
        }

        messageBox() = default;
        messageBox(int x, int y, int w, int h, shared_ptr<TTF_Font> msgFont, shared_ptr<TTF_Font> ttlFont,
                   const std::string& msg, const std::string& ttl = "Error")
                : rect{x, y, w, h}, font(msgFont), titleFont(ttlFont),
                  message(msg), title(ttl),
                  okButton(x + w - 100, y + h - 40, 80, 30, msgFont, "OK") {

            okButton.onClick = [this]() {
                visible = false;
            };
        }

        void show() {
            visible = true;
        }

        void hide() {
            visible = false;
        }

        void setMessage(const std::string& newMsg) {
            message = newMsg;
        }

        void setTitle(const std::string& newTitle) {
            title = newTitle;
        }

        bool handleEvent(const SDL_Event& e) {
            if (visible) {
                okButton.handleEvent(e);
                return true;
            }
            return false;
        }

        void draw(SDL_Renderer* renderer) {
            if (!visible) return;

            // پس‌زمینه
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);

            // حاشیه
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer, &rect);

            // عنوان
            if (titleFont && !title.empty()) {
                SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(titleFont.get(), title.c_str(), titleColor);
                if (titleSurf) {
                    SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
                    SDL_Rect titleRect{rect.x + 10, rect.y + 10, titleSurf->w, titleSurf->h};
                    SDL_RenderCopy(renderer, titleTex, nullptr, &titleRect);
                    SDL_FreeSurface(titleSurf);
                    SDL_DestroyTexture(titleTex);
                }
            }

            // متن پیام
            if (font && !message.empty()) {
                // محاسبه اندازه متن برای شکستن خطوط
                int maxWidth = rect.w - 40;
                std::vector<std::string> lines = wrapText(message, font.get(), maxWidth);

                int yPos = rect.y + 50;
                for (const auto& line : lines) {
                    SDL_Surface* msgSurf = TTF_RenderUTF8_Blended(font.get(), line.c_str(), textColor);
                    if (msgSurf) {
                        SDL_Texture* msgTex = SDL_CreateTextureFromSurface(renderer, msgSurf);
                        SDL_Rect msgRect{rect.x + 20, yPos, msgSurf->w, msgSurf->h};
                        SDL_RenderCopy(renderer, msgTex, nullptr, &msgRect);
                        SDL_FreeSurface(msgSurf);
                        SDL_DestroyTexture(msgTex);
                        yPos += msgSurf->h + 5;
                    }
                }
            }

            // دکمه OK
            okButton.draw(renderer);
        }

    private:
        // تابع برای شکستن متن به چند خط بر اساس عرض
        std::vector<std::string> wrapText(const std::string& text, TTF_Font* font, int maxWidth) {
            std::vector<std::string> lines;
            std::string currentLine;
            std::string currentWord;

            for (char c : text) {
                if (c == ' ' || c == '\n') {
                    if (!currentWord.empty()) {
                        // بررسی اگر اضافه کردن این کلمه به خط فعلی از عرض بیشتر نشود
                        std::string testLine = currentLine.empty() ? currentWord : currentLine + " " + currentWord;
                        int w, h;
                        TTF_SizeUTF8(font, testLine.c_str(), &w, &h);

                        if (w <= maxWidth) {
                            currentLine = testLine;
                        } else {
                            if (!currentLine.empty()) {
                                lines.push_back(currentLine);
                            }
                            currentLine = currentWord;
                        }
                        currentWord.clear();
                    }

                    if (c == '\n') {
                        if (!currentLine.empty()) {
                            lines.push_back(currentLine);
                            currentLine.clear();
                        }
                    }
                } else {
                    currentWord += c;
                }
            }

            // اضافه کردن آخرین کلمه
            if (!currentWord.empty()) {
                std::string testLine = currentLine.empty() ? currentWord : currentLine + " " + currentWord;
                int w, h;
                TTF_SizeUTF8(font, testLine.c_str(), &w, &h);

                if (w <= maxWidth) {
                    currentLine = testLine;
                } else {
                    if (!currentLine.empty()) {
                        lines.push_back(currentLine);
                    }
                    currentLine = currentWord;
                }
            }

            if (!currentLine.empty()) {
                lines.push_back(currentLine);
            }

            return lines;
        }
    };

    // کلاس PopupMenu (همان کد قبلی)
    class PopupMenu {
    public:
        SDL_Rect rect{0, 0, 180, 0};
        SDL_Color bgColor{245, 245, 245, 255};
        SDL_Color borderColor{60, 60, 60, 255};
        SDL_Color shadowColor{0, 0, 0, 80};
        bool visible = false;

        int padding = 6;
        int gap = 4;
        int itemHeight = 32;
        int minWidth = 140;

        std::vector<Button> items;
        shared_ptr<TTF_Font> font = nullptr;

        std::string fontPath;  // مسیر فونت برای سریالایز
        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز SDL_Rect
            ar(rect.x, rect.y, rect.w, rect.h);

            // سریالایز رنگ‌ها
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);

            // سریالایز وضعیت و اندازه‌ها
            ar(visible, padding, gap, itemHeight, minWidth);

            // سریالایز آیتم‌های منو
            ar(items);

            // سریالایز مسیر فونت
            ar(fontPath);
        }
        PopupMenu() = default;
        explicit PopupMenu(shared_ptr<TTF_Font> f, int width = 180)
                : font(f)
        {
            rect.w = width > 0 ? width : minWidth;
        }

        void addItem(const std::string& title, std::function<void()> onClick) {
            Button b(0, 0, rect.w - 2 * padding, itemHeight, font, title);
            b.onClick = [this, onClick]() {
                this->visible = false;
                if (onClick) onClick();
            };

            b.bgColor = {230, 230, 230, 255};
            b.hoverColor = {210, 210, 210, 255};
            b.pressedColor = {190, 190, 190, 255};
            b.disabledColor = {200, 200, 200, 255};
            b.textColor = {25, 25, 25, 255};
            b.borderThickness = 0;

            items.push_back(b);
            layout();
        }

        void setPosition(int x, int y) {
            rect.x = x;
            rect.y = y;
            layout();
        }

        void show()  { visible = true; }
        void hide()  { visible = false; }
        void toggle(){ visible = !visible; }

        bool handleEvent(const SDL_Event& e) {
            if (!visible) return false;

            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                hide();
                return true;
            }

            if (e.type == SDL_KEYDOWN) {
                for (auto& item : items) {
                    if (item.shortcutEnabled &&
                        e.key.keysym.sym == item.shortcutKey &&
                        (e.key.keysym.mod & item.shortcutMod) == item.shortcutMod) {
                        item.handleEvent(e);
                        return true;
                    }
                }
            }
            if (e.type == SDL_MOUSEMOTION ||
                e.type == SDL_MOUSEBUTTONDOWN ||
                e.type == SDL_MOUSEBUTTONUP) {

                SDL_Point p;
                if (e.type == SDL_MOUSEMOTION) p = { e.motion.x, e.motion.y };
                else                           p = { e.button.x, e.button.y };

                bool inside = SDL_PointInRect(&p, &rect);

                if (!inside && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)) {
                    hide();
                    return false;
                }

                if (inside) {
                    for (auto& it : items) it.handleEvent(e);
                    return true;
                }
            }

            return false;
        }

        void draw(SDL_Renderer* renderer) {
            if (!visible) return;

            SDL_Rect shadow{ rect.x + 2, rect.y + 2, rect.w, rect.h };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a);
            SDL_RenderFillRect(renderer, &shadow);

            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);

            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer, &rect);

            for (auto& it : items) it.draw(renderer);
        }

        void clampToWindow(int windowW, int windowH) {
            if (rect.x + rect.w > windowW) rect.x = windowW - rect.w;
            if (rect.y + rect.h > windowH) rect.y = windowH - rect.h;
            if (rect.x < 0) rect.x = 0;
            if (rect.y < 0) rect.y = 0;
            layout();
        }

    private:
        void layout() {
            int contentH = items.empty() ? 0
                                         : (int)items.size() * itemHeight + ((int)items.size() - 1) * gap;
            rect.h = padding * 2 + contentH;

            int x = rect.x + padding;
            int y = rect.y + padding;
            int w = rect.w - 2 * padding;

            for (size_t i = 0; i < items.size(); ++i) {
                items[i].rect = { x, y + (int)i * (itemHeight + gap), w, itemHeight };
            }
        }
    };

    // کلاس دیالوگ (با تغییرات جدید)
    class ElementDialog {
    public:
        SDL_Rect rect{0, 0, 350, 0}; // Initial width, height will be adjusted dynamically
        SDL_Color bgColor{240, 240, 240, 255};
        SDL_Color borderColor{100, 100, 100, 255};
        bool visible = false;
        bool dragging = false;
        int dragOffsetX = 0, dragOffsetY = 0;

        // Common fields
        TextBox nameBox;

        // Fields for basic elements
        TextBox valueBox;
        TextBox modelBox;

        // Fields for dependent sources
        TextBox gainBox;
        TextBox sourceElementBox;
        TextBox posNodeBox;
        TextBox negNodeBox;

        // Fields for signal sources
        TextBox phaseBox;
        TextBox bfrBox;
        TextBox tPulseBox;
        TextBox areaBox;
        TextBox frequencyBox;
        TextBox vOffsetBox;
        TextBox vAmplitudeBox;
        TextBox vInitialBox;
        TextBox vOnBox;
        TextBox tDelayBox;
        TextBox tRiseBox;
        TextBox tFallBox;
        TextBox tOnBox;
        TextBox tPeriodBox;
        TextBox nCyclesBox;

        Button okButton;
        Button cancelButton;
        std::shared_ptr<TTF_Font> font;

        std::string currentElementType;
        std::string title;
        std::function<void(const std::vector<std::string>&)> onOK;

        std::string fontPath;
        std::string callbackID;

        // Default constructor for serialization (initializes with nullptrs and 0s)
        ElementDialog() :
                nameBox(), valueBox(), modelBox(), gainBox(), sourceElementBox(),
                posNodeBox(), negNodeBox(), phaseBox(),bfrBox(), tPulseBox(), areaBox(), frequencyBox(),
                vOffsetBox(), vAmplitudeBox(), vInitialBox(), vOnBox(), tDelayBox(),
                tRiseBox(), tFallBox(), tOnBox(), tPeriodBox(), nCyclesBox(),
                okButton(), cancelButton() {}


        // Main constructor - initializes all UI components with a font
        ElementDialog(std::shared_ptr<TTF_Font> f) :
                font(f),
                // Initialize all text boxes with dummy positions; setPosition will correct them
                nameBox(0, 0, 200, 30, f),
                valueBox(0, 0, 200, 30, f),
                modelBox(0, 0, 200, 30, f),
                gainBox(0, 0, 200, 30, f),
                sourceElementBox(0, 0, 200, 30, f),
                posNodeBox(0, 0, 90, 30, f),
                negNodeBox(0, 0, 90, 30, f),
                phaseBox(0, 0, 200, 30, f),
                bfrBox(0,0,200,30,f),
                tPulseBox(0, 0, 200, 30, f),
                areaBox(0, 0, 200, 30, f),
                frequencyBox(0, 0, 200, 30, f),
                vOffsetBox(0, 0, 200, 30, f),
                vAmplitudeBox(0, 0, 200, 30, f),
                vInitialBox(0, 0, 200, 30, f),
                vOnBox(0, 0, 200, 30, f),
                tDelayBox(0, 0, 200, 30, f),
                tRiseBox(0, 0, 200, 30, f),
                tFallBox(0, 0, 200, 30, f),
                tOnBox(0, 0, 200, 30, f),
                tPeriodBox(0, 0, 200, 30, f),
                nCyclesBox(0, 0, 200, 30, f),
                okButton(0, 0, 120, 30, f, "OK"),
                cancelButton(0, 0, 120, 30, f, "Cancel")
        {
            // Set up button click handlers
            okButton.onClick = [this]() {
                if (onOK) {
                    std::vector<std::string> values;
                    values.push_back(nameBox.text); // Name is always included

                    // Add values based on currentElementType
                    if (currentElementType == "Resistor" || currentElementType == "Capacitor" ||
                        currentElementType == "Inductor" || currentElementType == "VoltageSource" ||
                        currentElementType == "CurrentSource") {
                        values.push_back(valueBox.text);
                    }
                    else if (currentElementType == "AcVoltageSource") {
                        values.push_back(valueBox.text);    // Amplitude
                        values.push_back(phaseBox.text);
                    }
                    else if (currentElementType == "PhaseVoltageSource") {
                        values.push_back(valueBox.text);    // Amplitude
                        values.push_back(bfrBox.text);
                    }
                    else if (currentElementType == "PWLVoltageSource") {
                    }
                    else if (currentElementType == "Diode") {
                        values.push_back(modelBox.text);
                    }
                    else if (currentElementType == "CCCS" || currentElementType == "CCVS") {
                        values.push_back(gainBox.text);
                        values.push_back(sourceElementBox.text);
                    }
                    else if (currentElementType == "VCCS" || currentElementType == "VCVS") {
                        values.push_back(gainBox.text);
                        values.push_back(posNodeBox.text);
                        values.push_back(negNodeBox.text);
                    }
                    else if (currentElementType == "DeltaVoltageSource" || currentElementType == "DeltaCurrentSource") {
                        values.push_back(tPulseBox.text);
                        values.push_back(areaBox.text);
                    }
                    else if (currentElementType == "SineVoltageSource" || currentElementType == "SineCurrentSource") {
                        values.push_back(frequencyBox.text);
                        values.push_back(vOffsetBox.text);
                        values.push_back(vAmplitudeBox.text);
                    }
                    else if (currentElementType == "PulseVoltageSource" || currentElementType == "PulseCurrentSource") {
                        values.push_back(vInitialBox.text);
                        values.push_back(vOnBox.text);
                        values.push_back(tDelayBox.text);
                        values.push_back(tRiseBox.text);
                        values.push_back(tFallBox.text);
                        values.push_back(tOnBox.text);
                        values.push_back(tPeriodBox.text);
                        values.push_back(nCyclesBox.text);
                    }
                    onOK(values);
                }
                visible = false;
            };

            cancelButton.onClick = [this]() {
                visible = false;
            };
        }

        // Serialization method (for saving/loading dialog state)
        template <class Archive>
        void serialize(Archive& ar) {
            ar(rect.x, rect.y, rect.w, rect.h);
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(visible, dragging, dragOffsetX, dragOffsetY);
            // Serialize all TextBox members
            ar(nameBox, valueBox, modelBox, gainBox, sourceElementBox,
               posNodeBox, negNodeBox, phaseBox,bfrBox, tPulseBox, areaBox, frequencyBox,
               vOffsetBox, vAmplitudeBox, vInitialBox, vOnBox, tDelayBox,
               tRiseBox, tFallBox, tOnBox, tPeriodBox, nCyclesBox);
            // Serialize Button members
            ar(okButton, cancelButton);
            // Serialize other string/state variables
            ar(fontPath, currentElementType, title, callbackID);
        }

        // Sets the position of the dialog and all its internal elements
        void setPosition(int x, int y) {
            rect.x = x;
            rect.y = y;

            int currentY = 50; // Starting Y position for the first field (Name)
            const int labelOffset = 20; // X position for labels
            const int textBoxOffset = 120; // X position for text boxes
            const int textBoxWidth = 200;
            const int fieldSpacing = 40; // Vertical space between fields

            // Position Name field (common to all)
            nameBox.rect.x = rect.x + textBoxOffset;
            nameBox.rect.y = rect.y + currentY;
            nameBox.rect.w = textBoxWidth;
            currentY += fieldSpacing;

            // Position other fields based on element type
            if (currentElementType == "Resistor" || currentElementType == "Capacitor" ||
                currentElementType == "Inductor" || currentElementType == "VoltageSource" ||
                currentElementType == "CurrentSource") {
                valueBox.rect.x = rect.x + textBoxOffset;
                valueBox.rect.y = rect.y + currentY;
                valueBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "AcVoltageSource") {
                valueBox.rect.x = rect.x + textBoxOffset;
                valueBox.rect.y = rect.y + currentY; // Amplitude
                valueBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                phaseBox.rect.x = rect.x + textBoxOffset;
                phaseBox.rect.y = rect.y + currentY; // Phase
                phaseBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "PhaseVoltageSource") {
                valueBox.rect.x = rect.x + textBoxOffset;
                valueBox.rect.y = rect.y + currentY; // Amplitude
                valueBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                bfrBox.rect.x = rect.x + textBoxOffset;
                bfrBox.rect.y = rect.y + currentY; // fr
                bfrBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "PWLVoltageSource") {}
            else if (currentElementType == "Diode") {
                modelBox.rect.x = rect.x + textBoxOffset;
                modelBox.rect.y = rect.y + currentY;
                modelBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "CCCS" || currentElementType == "CCVS") {
                gainBox.rect.x = rect.x + textBoxOffset;
                gainBox.rect.y = rect.y + currentY;
                gainBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                sourceElementBox.rect.x = rect.x + textBoxOffset;
                sourceElementBox.rect.y = rect.y + currentY;
                sourceElementBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "VCCS" || currentElementType == "VCVS") {
                gainBox.rect.x = rect.x + textBoxOffset;
                gainBox.rect.y = rect.y + currentY;
                gainBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                posNodeBox.rect.x = rect.x + textBoxOffset; // Adjusted for + node
                posNodeBox.rect.y = rect.y + currentY;
                posNodeBox.rect.w = 90; // Smaller width for nodes
                negNodeBox.rect.x = rect.x + textBoxOffset + 110; // Adjusted for - node
                negNodeBox.rect.y = rect.y + currentY;
                negNodeBox.rect.w = 90; // Smaller width for nodes
                currentY += fieldSpacing;
            }
            else if (currentElementType == "DeltaVoltageSource" || currentElementType == "DeltaCurrentSource") {
                tPulseBox.rect.x = rect.x + textBoxOffset;
                tPulseBox.rect.y = rect.y + currentY;
                tPulseBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                areaBox.rect.x = rect.x + textBoxOffset;
                areaBox.rect.y = rect.y + currentY;
                areaBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "SineVoltageSource" || currentElementType == "SineCurrentSource") {
                frequencyBox.rect.x = rect.x + textBoxOffset;
                frequencyBox.rect.y = rect.y + currentY;
                frequencyBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                vOffsetBox.rect.x = rect.x + textBoxOffset;
                vOffsetBox.rect.y = rect.y + currentY;
                vOffsetBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                vAmplitudeBox.rect.x = rect.x + textBoxOffset;
                vAmplitudeBox.rect.y = rect.y + currentY;
                vAmplitudeBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }
            else if (currentElementType == "PulseVoltageSource" || currentElementType == "PulseCurrentSource") {
                vInitialBox.rect.x = rect.x + textBoxOffset;
                vInitialBox.rect.y = rect.y + currentY;
                vInitialBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                vOnBox.rect.x = rect.x + textBoxOffset;
                vOnBox.rect.y = rect.y + currentY;
                vOnBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                tDelayBox.rect.x = rect.x + textBoxOffset;
                tDelayBox.rect.y = rect.y + currentY;
                tDelayBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                tRiseBox.rect.x = rect.x + textBoxOffset;
                tRiseBox.rect.y = rect.y + currentY;
                tRiseBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                tFallBox.rect.x = rect.x + textBoxOffset;
                tFallBox.rect.y = rect.y + currentY;
                tFallBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                tOnBox.rect.x = rect.x + textBoxOffset;
                tOnBox.rect.y = rect.y + currentY;
                tOnBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                tPeriodBox.rect.x = rect.x + textBoxOffset;
                tPeriodBox.rect.y = rect.y + currentY;
                tPeriodBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
                nCyclesBox.rect.x = rect.x + textBoxOffset;
                nCyclesBox.rect.y = rect.y + currentY;
                nCyclesBox.rect.w = textBoxWidth;
                currentY += fieldSpacing;
            }

            // Position buttons at the bottom of the dialog, relative to dialog's current position
            okButton.rect.x = rect.x + 50;
            okButton.rect.y = rect.y + rect.h - 50; // Position relative to dialog's bottom edge
            cancelButton.rect.x = rect.x + 200;
            cancelButton.rect.y = rect.y + rect.h - 50;
        }

        // Shows the dialog for a given element type
        void show(const std::string& elementType) {
            currentElementType = elementType;
            visible = true;

            // Clear all text fields when showing a new dialog
            nameBox.text = "";
            valueBox.text = "";
            modelBox.text = "";
            gainBox.text = "";
            sourceElementBox.text = "";
            posNodeBox.text = "";
            negNodeBox.text = "";
            phaseBox.text = "";
            bfrBox.text="";
            tPulseBox.text = "";
            areaBox.text = "";
            frequencyBox.text = "";
            vOffsetBox.text = "";
            vAmplitudeBox.text = "";
            vInitialBox.text = "";
            vOnBox.text = "";
            tDelayBox.text = "";
            tRiseBox.text = "";
            tFallBox.text = "";
            tOnBox.text = "";
            tPeriodBox.text = "";
            nCyclesBox.text = "";

            // Map element types to their display titles
            static std::unordered_map<std::string, std::string> titles = {
                    {"Resistor", "Resistor Properties"},
                    {"Capacitor", "Capacitor Properties"},
                    {"Inductor", "Inductor Properties"},
                    {"Diode", "Diode Properties"},
                    {"VoltageSource", "DC Voltage Source Properties"},
                    {"AcVoltageSource", "AC Voltage Source Properties"},
                    {"PhaseVoltageSource", "Phase Voltage Source Properties"},
                    {"PWLVoltageSource", "PWL Voltage Source Properties"},
                    {"CurrentSource", "DC Current Source Properties"},
                    {"CCCS", "CCCS Properties"},
                    {"CCVS", "CCVS Properties"},
                    {"VCCS", "VCCS Properties"},
                    {"VCVS", "VCVS Properties"},
                    {"DeltaVoltageSource", "Delta Voltage Source Properties"},
                    {"DeltaCurrentSource", "Delta Current Source Properties"},
                    {"SineVoltageSource", "Sine Voltage Source Properties"},
                    {"SineCurrentSource", "Sine Current Source Properties"},
                    {"PulseVoltageSource", "Pulse Voltage Source Properties"},
                    {"PulseCurrentSource", "Pulse Current Source Properties"}
            };

            title = titles[elementType];

            // Adjust dialog height based on the number of fields for the current element type
            if (elementType == "PulseVoltageSource" || elementType == "PulseCurrentSource") {
                rect.h = 450;
            }
            else if (elementType == "SineVoltageSource" || elementType == "SineCurrentSource") {
                rect.h = 250;
            }
            else if (elementType == "DeltaVoltageSource" || elementType == "DeltaCurrentSource" ||
                     elementType == "VCCS" || elementType == "VCVS" || elementType == "CCCS" ||
                     elementType == "CCVS" || elementType == "AcVoltageSource"||elementType == "PhaseVoltageSource") {
                rect.h = 200;
            }
            else {
                rect.h = 180; // Default height for elements with Name + Value/Model
            }

            // Set the dialog's initial position and then update all internal elements
            // This ensures the layout is correct from the first display.
            setPosition(100, 100); // Example initial position (you can adjust this)
        }

        // Hides the dialog
        void hide() { visible = false; }

        // Handles SDL events for the dialog and its children
        bool handleEvent(const SDL_Event& e) {
            if (!visible) return false;

            // Handle ESC key to close dialog
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                hide();
                return true;
            }

            SDL_Point mouse;
            if (e.type == SDL_MOUSEMOTION) {
                mouse = {e.motion.x, e.motion.y};
            } else {
                mouse = {e.button.x, e.button.y};
            }

            bool insideDialog = SDL_PointInRect(&mouse, &rect);

            // Click outside closes dialog (unless dragging started inside)
            if (!insideDialog && e.type == SDL_MOUSEBUTTONDOWN) {
                hide();
                return true;
            }

            // Handle dragging the dialog
            if (e.type == SDL_MOUSEBUTTONDOWN && insideDialog) {
                dragging = true;
                dragOffsetX = e.button.x - rect.x;
                dragOffsetY = e.button.y - rect.y;
            }
            if (e.type == SDL_MOUSEMOTION && dragging) {
                setPosition(e.motion.x - dragOffsetX, e.motion.y - dragOffsetY);
            }
            if (e.type == SDL_MOUSEBUTTONUP) {
                dragging = false;
            }

            // Pass events to child elements
            nameBox.handleEvent(e);

            if (currentElementType == "Resistor" || currentElementType == "Capacitor" ||
                currentElementType == "Inductor" || currentElementType == "VoltageSource" ||
                currentElementType == "CurrentSource") {
                valueBox.handleEvent(e);
            } else if (currentElementType == "AcVoltageSource") {
                valueBox.handleEvent(e);
                phaseBox.handleEvent(e);
            }
            else if (currentElementType == "PhaseVoltageSource") {
                valueBox.handleEvent(e);
                bfrBox.handleEvent(e);
            }
            else if (currentElementType == "Diode") {
                modelBox.handleEvent(e);
            } else if (currentElementType == "CCCS" || currentElementType == "CCVS") {
                gainBox.handleEvent(e);
                sourceElementBox.handleEvent(e);
            } else if (currentElementType == "VCCS" || currentElementType == "VCVS") {
                gainBox.handleEvent(e);
                posNodeBox.handleEvent(e);
                negNodeBox.handleEvent(e);
            } else if (currentElementType == "DeltaVoltageSource" || currentElementType == "DeltaCurrentSource") {
                tPulseBox.handleEvent(e);
                areaBox.handleEvent(e);
            } else if (currentElementType == "SineVoltageSource" || currentElementType == "SineCurrentSource") {
                frequencyBox.handleEvent(e);
                vOffsetBox.handleEvent(e);
                vAmplitudeBox.handleEvent(e);
            } else if (currentElementType == "PulseVoltageSource" || currentElementType == "PulseCurrentSource") {
                vInitialBox.handleEvent(e);
                vOnBox.handleEvent(e);
                tDelayBox.handleEvent(e);
                tRiseBox.handleEvent(e);
                tFallBox.handleEvent(e);
                tOnBox.handleEvent(e);
                tPeriodBox.handleEvent(e);
                nCyclesBox.handleEvent(e);
            }

            okButton.handleEvent(e);
            cancelButton.handleEvent(e);

            return true;
        }

        // Draws the dialog and all its internal elements
        void draw(SDL_Renderer* renderer) {
            if (!visible) return;

            // Draw background and border
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer, &rect);

            // Draw title
            SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(font.get(), title.c_str(), {0, 0, 0, 255});
            if (titleSurf) {
                SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
                SDL_Rect titleRect{rect.x + (rect.w - titleSurf->w) / 2, rect.y + 10, titleSurf->w, titleSurf->h};
                SDL_RenderCopy(renderer, titleTex, nullptr, &titleRect);
                SDL_FreeSurface(titleSurf);
                SDL_DestroyTexture(titleTex);
            }

            int currentY = 50; // Starting Y position for labels
            const int labelX = 20; // X position for labels

            // Draw Name label (common for all elements)
            drawLabel(renderer, "Name:", labelX, currentY);
            nameBox.draw(renderer);
            currentY += 40; // Increment for the next field

            // Draw fields and labels based on element type
            if (currentElementType == "Resistor" || currentElementType == "Capacitor" ||
                currentElementType == "Inductor" || currentElementType == "VoltageSource" ||
                currentElementType == "CurrentSource") {
                drawLabel(renderer, "Value:", labelX, currentY);
                valueBox.draw(renderer);
            }
            else if (currentElementType == "AcVoltageSource") {
                drawLabel(renderer, "Amplitude:", labelX, currentY);
                valueBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Phase:", labelX, currentY);
                phaseBox.draw(renderer);
            }
            else if (currentElementType == "PhaseVoltageSource") {
                drawLabel(renderer, "Amplitude:", labelX, currentY);
                valueBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "base Freq (Hz):", labelX, currentY);
                bfrBox.draw(renderer);
            }
            else if (currentElementType == "PWLVoltageSource") {
            }
            else if (currentElementType == "Diode") {
                drawLabel(renderer, "Model:", labelX, currentY);
                modelBox.draw(renderer);
            }
            else if (currentElementType == "CCCS" || currentElementType == "CCVS") {
                drawLabel(renderer, "Gain:", labelX, currentY);
                gainBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Element:", labelX, currentY);
                sourceElementBox.draw(renderer);
            }
            else if (currentElementType == "VCCS" || currentElementType == "VCVS") {
                drawLabel(renderer, "Gain:", labelX, currentY);
                gainBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Nodes:", labelX, currentY);
                drawLabel(renderer, "+", 100, currentY); // Specific label for pos node
                drawLabel(renderer, "-", 210, currentY); // Specific label for neg node
                posNodeBox.draw(renderer);
                negNodeBox.draw(renderer);
            }
            else if (currentElementType == "DeltaVoltageSource" || currentElementType == "DeltaCurrentSource") {
                drawLabel(renderer, "tPulse:", labelX, currentY);
                tPulseBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Area:", labelX, currentY);
                areaBox.draw(renderer);
            }
            else if (currentElementType == "SineVoltageSource" || currentElementType == "SineCurrentSource") {
                drawLabel(renderer, "Freq (Hz):", labelX, currentY);
                frequencyBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Voffset:", labelX, currentY);
                vOffsetBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Vamplitude:", labelX, currentY);
                vAmplitudeBox.draw(renderer);
            }
            else if (currentElementType == "PulseVoltageSource" || currentElementType == "PulseCurrentSource") {
                drawLabel(renderer, "Vinitial:", labelX, currentY);
                vInitialBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Von:", labelX, currentY);
                vOnBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Tdelay:", labelX, currentY);
                tDelayBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Trise:", labelX, currentY);
                tRiseBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Tfall:", labelX, currentY);
                tFallBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Ton:", labelX, currentY);
                tOnBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Tperiod:", labelX, currentY);
                tPeriodBox.draw(renderer);
                currentY += 40;
                drawLabel(renderer, "Ncycles:", labelX, currentY);
                nCyclesBox.draw(renderer);
            }

            // Draw buttons at the bottom
            okButton.draw(renderer);
            cancelButton.draw(renderer);
        }

    private:
        // Helper function to draw labels relative to the dialog's position
        void drawLabel(SDL_Renderer* renderer, const std::string& text, int xOffset, int yOffset) {
            SDL_Surface* labelSurf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), {0, 0, 0, 255});
            if (labelSurf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, labelSurf);
                SDL_Rect dst{rect.x + xOffset, rect.y + yOffset, labelSurf->w, labelSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_FreeSurface(labelSurf);
                SDL_DestroyTexture(tex);
            }
        }
    };

    class AnalyzeDialog{
    public:
        SDL_Rect rect{0, 0, 350, 500};  // Size adjusted based on content
        SDL_Color bgColor{240, 240, 240, 255};
        SDL_Color borderColor{100, 100, 100, 255};
        bool visible = false;
        bool dragging = false;
        int dragOffsetX = 0, dragOffsetY = 0;

        //Transient
        TextBox StopTime;
        TextBox StartTime;
        TextBox TimeStep;

        //DC sweep
        TextBox Source;
        TextBox startValue;
        TextBox EndValue;
        TextBox ValueStep;

        //AC Sweep
        TextBox TypeOfSweep;
        //Octave
        //Decade
        //Linear
        TextBox StartFrequency;
        TextBox StopFrequency;
        TextBox NumberOfPoints;

        //PhaseSweep
        TextBox BaseFrequency;
        TextBox StartPhase;
        TextBox StopPhase;
        TextBox NumberOfPointsP;

        Button okButton;
        Button cancelButton;
        shared_ptr<TTF_Font> font;

        std::string currentAnalyzeType;
        std::string title;

        std::function<void(const std::vector<std::string>&)> onOK;
        std::string fontPath; // برای ذخیره مسیر فونت
        std::string callbackID; // برای ذخیره شناسه callback

        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز هندسه و رنگ‌ها
            ar(rect.x, rect.y, rect.w, rect.h);
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);

            // سریالایز وضعیت
            ar(visible, dragging, dragOffsetX, dragOffsetY);

            // سریالایز تمام TextBoxهای تحلیل
            ar(StopTime, StartTime, TimeStep,
               Source, startValue, EndValue, ValueStep,
               TypeOfSweep, StartFrequency, StopFrequency, NumberOfPoints,
               BaseFrequency, StartPhase, StopPhase, NumberOfPointsP);

            // سریالایز دکمه‌ها
            ar(okButton, cancelButton);

            // سریالایز فونت و متغیرهای متنی
            ar(fontPath, currentAnalyzeType, title, callbackID);
        }
        AnalyzeDialog(){}
        AnalyzeDialog(shared_ptr<TTF_Font> f) :
                font(f),
                StopTime(120, 90, 200, 30, f),
                StartTime(120, 130, 200, 30, f),
                TimeStep(120, 170, 200, 30, f),
                Source(120, 90, 200, 30, f),
                startValue(120, 130, 200, 30, f),
                EndValue(120, 170, 200, 30, f),
                ValueStep(120, 210, 200, 30, f),
                TypeOfSweep(120, 90, 200, 30, f),
                StartFrequency(120, 130, 200, 30, f),
                StopFrequency(120, 170, 200, 30, f),
                NumberOfPoints(120, 210, 200, 30, f),
                BaseFrequency(120, 90, 200, 30, f),
                StartPhase(120, 130, 200, 30, f),
                StopPhase(120, 170, 200, 30, f),
                NumberOfPointsP(120, 210, 200, 30, f),
                okButton(50, 450, 120, 30, f, "OK"),
                cancelButton(200, 450, 120, 30, f, "Cancel")
        {
            okButton.onClick = [this]() {
                if(onOK) {
                    std::vector<std::string> values;

                    if(currentAnalyzeType == "Transient") {
                        values.push_back(StartTime.text);
                        values.push_back(StopTime.text);
                        values.push_back(TimeStep.text);
                    }
                    else if(currentAnalyzeType == "DC Sweep") {
                        values.push_back(Source.text);
                        values.push_back(startValue.text);
                        values.push_back(EndValue.text);
                        values.push_back(ValueStep.text);
                    }
                    else if(currentAnalyzeType == "AC Sweep") {
                        values.push_back(TypeOfSweep.text);
                        values.push_back(StartFrequency.text);
                        values.push_back(StopFrequency.text);
                        values.push_back(NumberOfPoints.text);
                    }
                    else if(currentAnalyzeType == "Phase Sweep") {
                        values.push_back(BaseFrequency.text);
                        values.push_back(StartPhase.text);
                        values.push_back(StopPhase.text);
                        values.push_back(NumberOfPointsP.text);
                    }

                    onOK(values);
                }
                visible = false;
            };

            cancelButton.onClick = [this]() {
                visible = false;
            };
        }

        void setPosition(int x, int y) {
            rect.x = x;
            rect.y = y;

            // Position fields based on analysis type
            if(currentAnalyzeType == "Transient") {
                StartTime.rect.x = x + 120;
                StartTime.rect.y = y + 90;
                StopTime.rect.x = x + 120;
                StopTime.rect.y = y + 130;
                TimeStep.rect.x = x + 120;
                TimeStep.rect.y = y + 170;
            }
            else if(currentAnalyzeType == "DC Sweep") {
                Source.rect.x = x + 120;
                Source.rect.y = y + 90;
                startValue.rect.x = x + 120;
                startValue.rect.y = y + 130;
                EndValue.rect.x = x + 120;
                EndValue.rect.y = y + 170;
                ValueStep.rect.x = x + 120;
                ValueStep.rect.y = y + 210;
            }
            else if(currentAnalyzeType == "AC Sweep") {
                TypeOfSweep.rect.x = x + 120;
                TypeOfSweep.rect.y = y + 90;
                StartFrequency.rect.x = x + 120;
                StartFrequency.rect.y = y + 130;
                StopFrequency.rect.x = x + 120;
                StopFrequency.rect.y = y + 170;
                NumberOfPoints.rect.x = x + 120;
                NumberOfPoints.rect.y = y + 210;
            }
            else if(currentAnalyzeType == "Phase Sweep") {
                BaseFrequency.rect.x = x + 120;
                BaseFrequency.rect.y = y + 90;
                StartPhase.rect.x = x + 120;
                StartPhase.rect.y = y + 130;
                StopPhase.rect.x = x + 120;
                StopPhase.rect.y = y + 170;
                NumberOfPointsP.rect.x = x + 120;
                NumberOfPointsP.rect.y = y + 210;
            }

            // Position buttons at the bottom of the dialog
            okButton.rect.x = x + 50;
            okButton.rect.y = y + rect.h - 50;
            cancelButton.rect.x = x + 200;
            cancelButton.rect.y = y + rect.h - 50;
        }

        void show(const std::string& analyzeType) {
            currentAnalyzeType = analyzeType;
            visible = true;

            // Clear all fields
            StartTime.text = "";
            StopTime.text = "";
            TimeStep.text = "";
            Source.text = "";
            startValue.text = "";
            EndValue.text = "";
            ValueStep.text = "";
            TypeOfSweep.text = "";
            StartFrequency.text = "";
            StopFrequency.text = "";
            NumberOfPoints.text = "";
            BaseFrequency.text = "";
            StartPhase.text = "";
            StopPhase.text = "";
            NumberOfPointsP.text = "";

            // Set title based on analysis type
            static std::unordered_map<std::string, std::string> titles = {
                    {"Transient", "Transient Analysis Parameters"},
                    {"DC Sweep", "DC Sweep Parameters"},
                    {"AC Sweep", "AC Sweep Parameters"},
                    {"Phase Sweep", "Phase Sweep Parameters"}
            };

            title = titles[analyzeType];

            // Adjust dialog height based on analysis type
            if(analyzeType == "Transient") {
                rect.h = 250;
            }
            else if(analyzeType == "DC Sweep") {
                rect.h = 280;
            }
            else if(analyzeType == "AC Sweep") {
                rect.h = 280;
            }
            else if(analyzeType == "Phase Sweep") {
                rect.h = 280;
            }
        }

        void hide() {
            visible = false;
        }

        bool handleEvent(const SDL_Event& e) {
            if(!visible) return false;

            // Handle ESC key to close dialog
            if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                hide();
                return true;
            }

            // Handle mouse events
            if(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION) {
                SDL_Point mouse;
                if(e.type == SDL_MOUSEMOTION) {
                    mouse = {e.motion.x, e.motion.y};
                } else {
                    mouse = {e.button.x, e.button.y};
                }

                bool inside = SDL_PointInRect(&mouse, &rect);

                // Click outside closes dialog
                if(!inside && e.type == SDL_MOUSEBUTTONDOWN) {
                    hide();
                    return true;
                }
                if (e.type == SDL_MOUSEBUTTONDOWN && SDL_PointInRect(&mouse, &rect)) {
                    dragging = true;
                    dragOffsetX = e.button.x - rect.x;
                    dragOffsetY = e.button.y - rect.y;
                }
                if (e.type == SDL_MOUSEMOTION && dragging) {
                    setPosition(e.motion.x - dragOffsetX, e.motion.y - dragOffsetY);
                }
                if (e.type == SDL_MOUSEBUTTONUP) {
                    dragging = false;
                }
            }

            // Pass events to appropriate input fields based on current analysis type
            if(currentAnalyzeType == "Transient") {
                StartTime.handleEvent(e);
                StopTime.handleEvent(e);
                TimeStep.handleEvent(e);
            }
            else if(currentAnalyzeType == "DC Sweep") {
                Source.handleEvent(e);
                startValue.handleEvent(e);
                EndValue.handleEvent(e);
                ValueStep.handleEvent(e);
            }
            else if(currentAnalyzeType == "AC Sweep") {
                TypeOfSweep.handleEvent(e);
                StartFrequency.handleEvent(e);
                StopFrequency.handleEvent(e);
                NumberOfPoints.handleEvent(e);
            }
            else if(currentAnalyzeType == "Phase Sweep") {
                BaseFrequency.handleEvent(e);
                StartPhase.handleEvent(e);
                StopPhase.handleEvent(e);
                NumberOfPointsP.handleEvent(e);
            }

            // Handle button events
            okButton.handleEvent(e);
            cancelButton.handleEvent(e);

            return true;
        }

        void draw(SDL_Renderer* renderer) {
            if(!visible) return;

            // Draw background and border
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer, &rect);

            // Draw title
            SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(font.get(), title.c_str(), {0, 0, 0, 255});
            if(titleSurf) {
                SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
                SDL_Rect titleRect{rect.x + (rect.w - titleSurf->w)/2, rect.y + 10, titleSurf->w, titleSurf->h};
                SDL_RenderCopy(renderer, titleTex, nullptr, &titleRect);
                SDL_FreeSurface(titleSurf);
                SDL_DestroyTexture(titleTex);
            }

            // Draw fields based on analysis type
            if(currentAnalyzeType == "Transient") {
                drawLabel(renderer, "Start Time:", 20, 90);
                drawLabel(renderer, "Stop Time:", 20, 130);
                drawLabel(renderer, "Time Step:", 20, 170);
                StartTime.draw(renderer);
                StopTime.draw(renderer);
                TimeStep.draw(renderer);
            }
            else if(currentAnalyzeType == "DC Sweep") {
                drawLabel(renderer, "Source:", 20, 90);
                drawLabel(renderer, "Start Value:", 20, 130);
                drawLabel(renderer, "End Value:", 20, 170);
                drawLabel(renderer, "Value Step:", 20, 210);
                Source.draw(renderer);
                startValue.draw(renderer);
                EndValue.draw(renderer);
                ValueStep.draw(renderer);
            }
            else if(currentAnalyzeType == "AC Sweep") {
                drawLabel(renderer, "Sweep Type:", 20, 90);
                drawLabel(renderer, "(dec/log)", 20, 100);
                drawLabel(renderer, "Start Freq (Hz):", 20, 130);
                drawLabel(renderer, "Stop Freq (Hz):", 20, 170);
                drawLabel(renderer, "Points:", 20, 210);

                // Draw dropdown for sweep type
                TypeOfSweep.draw(renderer);

                // Draw frequency and points inputs
                StartFrequency.draw(renderer);
                StopFrequency.draw(renderer);
                NumberOfPoints.draw(renderer);
            }
            else if(currentAnalyzeType == "Phase Sweep") {
                drawLabel(renderer, "Base Freq:", 20, 90);
                drawLabel(renderer, "Start Phase:", 20, 130);
                drawLabel(renderer, "Stop Phase:", 20, 170);
                drawLabel(renderer, "Points:", 20, 210);
                BaseFrequency.draw(renderer);
                StartPhase.draw(renderer);
                StopPhase.draw(renderer);
                NumberOfPointsP.draw(renderer);
            }

            // Draw buttons
            okButton.draw(renderer);
            cancelButton.draw(renderer);
        }

    private:
        void drawLabel(SDL_Renderer* renderer, const std::string& text, int x, int y) {
            SDL_Surface* labelSurf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), {0, 0, 0, 255});
            if(labelSurf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, labelSurf);
                SDL_Rect dst{rect.x + x, rect.y + y, labelSurf->w, labelSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_FreeSurface(labelSurf);
                SDL_DestroyTexture(tex);
            }
        }

    };

    class LabelDialog {
    public:
        SDL_Rect rect;
        bool visible = false;
        std::string labelText;
        TextBox textBox;
        Button okButton;
        Button cancelButton;
        shared_ptr<TTF_Font> font;
        std::string fontPath; // برای ذخیره مسیر فونت

        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز SDL_Rect (ذخیره به صورت چهار عدد صحیح)
            ar(rect.x, rect.y, rect.w, rect.h);

            // سریالایز وضعیت و متن
            ar(visible, labelText);

            // سریالایز کامپوننت‌ها
            ar(textBox, okButton, cancelButton);

            // سریالایز مسیر فونت
            ar(fontPath);
        }

        LabelDialog(){}
        LabelDialog(int x, int y, shared_ptr<TTF_Font> f) :
                rect{x, y, 300, 150},
                textBox(x + 20, y + 50, 260, 30, f),
                okButton(x + 50, y + 100, 80, 30, f, "OK"),
                cancelButton(x + 170, y + 100, 80, 30, f, "Cancel"),
                font(f)
        {
            okButton.onClick = [this]() {
                labelText = textBox.text;
                visible = false;
                textBox.text = "";
            };

            cancelButton.onClick = [this]() {
                visible = false;
                textBox.text = "";
            };
        }

        void show() {
            visible = true;
            textBox.active = true;
        }

        bool handleEvent(const SDL_Event& e) {
            if (!visible) return false;

            textBox.handleEvent(e);
            okButton.handleEvent(e);
            cancelButton.handleEvent(e);

            // ESC برای بستن دیالوگ
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                visible = false;
                textBox.text = "";
            }
            return true;
        }

        void draw(SDL_Renderer* renderer) {
            if (!visible) return;

            // پس‌زمینه دیالوگ
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
            SDL_RenderFillRect(renderer, &rect);

            // حاشیه دیالوگ
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(renderer, &rect);

            // عنوان دیالوگ
            if (font) {
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), "Enter Label Text", {0, 0, 0, 255});
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect dst{rect.x + 20, rect.y + 15, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_FreeSurface(surf);
                SDL_DestroyTexture(tex);
            }

            // رسم کامپوننت‌ها
            textBox.draw(renderer);
            okButton.draw(renderer);
            cancelButton.draw(renderer);
        }
    };

    class LabelNet {
    public:
        SDL_Rect rect;
        SDL_Rect baseRect;
        std::string text;
        bool isSelected = false;
        bool isPlacing = false;
        bool isDragging = false;
        int dragOffsetX = 0;
        int dragOffsetY = 0;
        shared_ptr<TTF_Font> font;
        SDL_Color bgColor{255, 255, 200, 255}; // رنگ زرد روشن
        SDL_Color textColor{0, 0, 0, 255};
        SDL_Color borderColor{0, 0, 0, 255};
        SDL_Color selectedColor{0, 120, 215, 255};

        static shared_ptr<LabelNet> placingInstance;
        std::string fontPath="assets/Tahoma.ttf"; // برای ذخیره مسیر فونت

        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز مستطیل‌ها
            ar(rect.x, rect.y, rect.w, rect.h);
            ar(baseRect.x, baseRect.y, baseRect.w, baseRect.h);

            // سریالایز متن و وضعیت
            ar(text, isSelected, isPlacing, isDragging, dragOffsetX, dragOffsetY);

            // سریالایز رنگ‌ها
            ar(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            ar(textColor.r, textColor.g, textColor.b, textColor.a);
            ar(borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            ar(selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a);

            // سریالایز مسیر فونت
            ar(fontPath);
        }

        LabelNet(){
            font= shared_ptr<TTF_Font>(TTF_OpenFont(fontPath.c_str(), 14), TTF_CloseFont);
        }
        LabelNet(int x, int y, const std::string& t, shared_ptr<TTF_Font> f) : text(t), font(f) {
            snapToGrid(x, y); // مطمئن می‌شویم روی شبکه قرار می‌گیرد
            updatePosition(x, y);
        }

        void updatePosition(int x, int y) {
            snapToGrid(x, y);
            rect = {x, y, 30, 15}; // اندازه مستطیل اصلی
            // محاسبه موقعیت baseRect (پایین مستطیل اصلی + فاصله ۱۰ پیکسل)
            int baseX = x + (rect.w / 2); // وسط مستطیل اصلی
            int baseY = y + rect.h;  // زیر مستطیل اصلی + فاصله ۱۰ پیکسل
            snapToGrid(baseX, baseY); // پایه روی شبکه قرار می‌گیرد
            baseRect = {baseX - 5+1, baseY - 5+1, 10, 10}; // مرکز پایه روی نقطه شبکه
        }

        void RenameNodes(){
            shared_ptr<Node> base=Wire::findNode(baseRect.x+5-1,baseRect.y+5-1);
            if(!base){
                return;
            }

            if (!base->getIsGround()) {
                // پیدا کردن تمام گره‌های هم‌نام (که newCircuit آنها را یکسان کرده)
                std::string targetName = base->name;
                for (auto& line : Wire::Lines) {
                    for (auto& node : line->Nodes) {
                        if (node && node->name == targetName) {
                            node->name = text;
                        }
                    }
                }
                base->name = text;
            }
            else {
                // برای زمین کردن
                std::string targetName = base->name;
                for (auto& line : Wire::Lines) {
                    for (auto& node : line->Nodes) {
                        if (node && node->name == targetName) {
                            node->setIsGround(true);
                            node->name = "N00";
                        }
                    }
                }
                base->name = "N00";
            }
        }

        void handleEvent(const SDL_Event& e) {
            if (isPlacing) {
                // حالت قرار دادن جدید
                if (e.type == SDL_MOUSEMOTION) {
                    int mouseX = e.motion.x;
                    int mouseY = e.motion.y;
                    updatePosition(mouseX, mouseY);
                }
                else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                    // کلیک چپ برای تثبیت موقعیت
                    isPlacing = false;
                    isSelected = true;

                    // ایجاد یک لیبل جدید برای قرار دادن بعدی
                    placingInstance =  make_shared<LabelNet>(e.button.x, e.button.y, text, font);
                    placingInstance->isPlacing = true;
                }
                else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                    // ESC برای لغو
                    isPlacing = false;
                    if (placingInstance.get() == this) {
                        placingInstance = nullptr;
                    }
                }
            }
            else {
                // حالت عادی
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                    SDL_Point mouse{e.button.x, e.button.y};
                    if (SDL_PointInRect(&mouse, &rect) ){
                        isSelected = true;
                        isDragging = true;
                        dragOffsetX = e.button.x - rect.x;
                        dragOffsetY = e.button.y - rect.y;
                    }
                    else {
                        isSelected = false;
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                    isDragging = false;
                }
                else if (e.type == SDL_MOUSEMOTION && isDragging) {
                    rect.x = e.motion.x - dragOffsetX;
                    rect.y = e.motion.y - dragOffsetY;
                    baseRect.x = rect.x + 25;
                    baseRect.y = rect.y + 20;
                }
            }
        }

        void draw(SDL_Renderer* renderer) {
            // رسم مستطیل اصلی
            SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(renderer, &rect);

            // رسم حاشیه (آبی اگر انتخاب شده، سیاه اگر نه)
            if (isSelected || isPlacing) {
                SDL_SetRenderDrawColor(renderer, selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a);
            }
            else {
                SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
                //RenameNodes();
            }
            SDL_RenderDrawRect(renderer, &rect);

            // رسم پایه
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderFillRect(renderer, &baseRect);

            // رسم متن
            if (!text.empty() && font) {
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), text.c_str(), textColor);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst{
                            rect.x + (rect.w - surf->w) / 2,
                            rect.y + (rect.h - surf->h) / 2,
                            surf->w, surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
            }
        }

    };
// تعریف متغیر استاتیک
    shared_ptr<LabelNet> LabelNet::placingInstance = nullptr;

    class GNDSymbol {
    public:
        SDL_Rect bounds;
        SDL_Rect baseRect;
        bool isPlacing;
        static shared_ptr<GNDSymbol> placingInstance;

        template <class Archive>
        void serialize(Archive& ar) {
            // سریالایز مستطیل‌ها
            ar(bounds.x, bounds.y, bounds.w, bounds.h);
            ar(baseRect.x, baseRect.y, baseRect.w, baseRect.h);

            // سریالایز وضعیت
            ar(isPlacing);
        }

        GNDSymbol(){}
        GNDSymbol(int x, int y) : isPlacing(true) {
            placingInstance = shared_ptr<GNDSymbol>(this);
            updatePosition(x, y);
        }

        ~GNDSymbol() {
            if (placingInstance.get() == this) {
                placingInstance = nullptr;
            }
        }

        void updatePosition(int x, int y) {
            snapToGrid(x, y);
            baseRect = {x - 5+1, y - 5+1, 10, 10};
            bounds = {x - 15, y + 5, 30, 15};
        }

        // تابع تشخیص نقطه در مثلث (خصوصی)
        bool pointInTriangle(SDL_Point p, SDL_Point p0, SDL_Point p1, SDL_Point p2) const {
            // محاسبه بردارها
            float s = p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y;
            float t = p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y;

            if ((s < 0) != (t < 0))
                return false;

            float A = -p1.y * p2.x + p0.y * (p2.x - p1.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y;

            if (A < 0.0) {
                s = -s;
                t = -t;
                A = -A;
            }

            return s > 0 && t > 0 && (s + t) <= A;
        }

        // تابع بررسی کلیک داخل مثلث (عمومی)
        bool containsPoint(int x, int y) const {
            SDL_Point clickPoint = {x, y};

            // تعریف نقاط مثلث
            SDL_Point p0 = {bounds.x, bounds.y}; // گوشه چپ بالا
            SDL_Point p1 = {bounds.x + bounds.w, bounds.y}; // گوشه راست بالا
            SDL_Point p2 = {bounds.x + bounds.w/2, bounds.y + bounds.h}; // نقطه پایین وسط

            return pointInTriangle(clickPoint, p0, p1, p2);
        }

        void draw(SDL_Renderer* ren) const {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_Point triangle[4] = {
                    {bounds.x, bounds.y},                // گوشه چپ بالا
                    {bounds.x + bounds.w, bounds.y},      // گوشه راست بالا
                    {bounds.x + bounds.w/2, bounds.y + bounds.h},  // نقطه پایین وسط
                    {bounds.x, bounds.y}                 // برگشت به نقطه شروع
            };
            SDL_RenderDrawLines(ren, triangle, 4);

            SDL_SetRenderDrawColor(ren, 0, 0, 255, 255);
            SDL_RenderFillRect(ren, &baseRect);
        }

        void handleEvent(SDL_Event& e) {
            if (!isPlacing) return;

            switch (e.type) {
                case SDL_MOUSEMOTION:
                    updatePosition(e.motion.x, e.motion.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        isPlacing = false;
                    }
                    break;
            }
        }

        void RenameNode(){
            shared_ptr<Node> base=Wire::findNode(baseRect.x+5-1,baseRect.y+5-1);
            if(!base) {
                return;
            }

            // ذخیره نام اصلی قبل از تغییر
            std::string targetName = base->name;

            // علامت گذاری تمام نودهای متصل به عنوان زمین
            for (auto& line : Wire::Lines) {
                for (auto& node : line->Nodes) {
                    if (node && node->name == targetName) {
                        node->setIsGround(true);
                        node->name = "N00"; // استفاده از نام ثابت برای زمین
                    }
                }
            }
            // به روز رسانی نود پایه
            base->setIsGround(true);
            base->name = "N00";
        }
    };

    shared_ptr<GNDSymbol> GNDSymbol::placingInstance = nullptr;

    std::string ShowSaveDialog(HWND hwnd) {
        OPENFILENAME ofn = {0};
        char szFile[260] = {0};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Circuit Files\0*.shirali\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = "cir"; // پسوند اختصاصی برای فایل‌های مدار
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileName(&ofn)) {
            return szFile;
        }
        return "";
    }

    string ShowOpenFileDialog(HWND hwnd) {
        OPENFILENAME ofn = {0};
        char szFile[MAX_PATH] = {0};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Circuit Files\0*.shirali\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileName(&ofn)) {
            return szFile;
        }
        return "";
    }

}
using namespace graphicElements;

//////---------------------------------------
namespace Analyze{
    bool allVisited(const vector<bool>& xx) {
        for (bool b : xx) {
            if (!b) return false;
        }
        return true;
    }
    void dfs(int i, const vector<vector<int>>& x, vector<bool>& xx) {
        xx[i] = true;
        for (int j = 0; j < (int)x[i].size(); ++j) {
            if (x[i][j] == 1 && !xx[j]) {
                dfs(j, x, xx);
            }
        }
    }
    bool discontinuousCircuit(vector<shared_ptr<Element>>& elements, vector<shared_ptr<Node>>& nodes) {
        if (nodes.empty() || elements.empty()) return false;

        // Create a set of nodes that are connected to elements
        unordered_set<string> connectedNodes;
        for (auto& elem : elements) {
            connectedNodes.insert(elem->getNodeN()->getName());
            connectedNodes.insert(elem->getNodeP()->getName());
        }

// If no nodes are connected, circuit is discontinuous
        if (connectedNodes.empty()) return true;

// Create mapping only for connected nodes
        map<string, int> nodeIndexMap;
        vector<string> indexedNodes;
        for (auto& node : nodes) {
            if (connectedNodes.count(node->getName())) {
                nodeIndexMap[node->getName()] = indexedNodes.size();
                indexedNodes.push_back(node->getName());
            }
        }

// Create adjacency matrix only for connected nodes
        int n = indexedNodes.size();
        vector<vector<int>> adjMatrix(n, vector<int>(n, 0));
        vector<bool> visited(n, false);

        for (auto& elem : elements) {
            int u = nodeIndexMap[elem->getNodeN()->getName()];
            int v = nodeIndexMap[elem->getNodeP()->getName()];
            adjMatrix[u][v] = 1;
            adjMatrix[v][u] = 1;
        }

// Start DFS from first connected node
        dfs(0, adjMatrix, visited);

// Check if all connected nodes were visited
        for (bool v : visited) {
            if (!v) return true; // Found unvisited connected node - discontinuous
        }
        return false; // All connected nodes visited - continuous
    }
    bool notFoundGnd(){
        int n=0;
        for (auto &i:Wire::allNodes) {
            if(i->getIsGround())
                n++;
        }
        return (n!=0);
    }
    void findError(vector<shared_ptr<Element>>& elements,vector<shared_ptr<Node>>& nodes) {
        if (!discontinuousCircuit(elements,nodes)) {
            throw DiscontinuousCircuit();
        }
        if (!notFoundGnd()) {
            throw NotFoundGND();
        }
    }
    vector<double> solveLinearSystem(vector<vector<double>>& A, vector<double>& z) {
        const double EPSILON = 1e-12;
        int n = A.size();
        for (int i = 0; i < n; i++) {
            int maxRow = i;
            for (int k = i + 1; k < n; k++) {
                if (abs(A[k][i]) > abs(A[maxRow][i])) {
                    maxRow = k;
                }
            }
            if (maxRow != i) {
                swap(A[i], A[maxRow]);
                swap(z[i], z[maxRow]);
            }
            if (abs(A[i][i]) < EPSILON) {
                A[i][i] += EPSILON;
                cerr << "Warning: Matrix was nearly singular, added small value to diagonal\n";
            }
            double diag = A[i][i];
            for (int j = i; j < n; j++) {
                A[i][j] /= diag;
            }
            z[i] /= diag;
            for (int k = 0; k < n; k++) {
                if (k != i && abs(A[k][i]) > EPSILON) {
                    double factor = A[k][i];
                    for (int j = i; j < n; j++) {
                        A[k][j] -= factor * A[i][j];
                    }
                    z[k] -= factor * z[i];
                }
            }
        }
        return z;
    }

    double unitHandler3(string input, string type) {
        regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
        smatch match;
        if (regex_match(input, match, pattern)) {
            double value = stod(match[1]);
            string unit = match[2].str();
            double multiplier = 1.0;
            if (unit == "n") multiplier = 1e-9;
            else if (unit == "u") multiplier = 1e-6;
            else if (unit == "m") multiplier = 1e-3;
            else if (unit == "k") multiplier = 1e3;
            else if (unit == "M" || unit == "Meg") multiplier = 1e6;
            else if (unit == "G") multiplier = 1e9;
            return value * multiplier;
        } else {
            throw invalidValue(type);
        }
    }

    vector<complex<double>> solveLinearSystemComplex(vector<vector<complex<double>>> &A,
                                                     vector<complex<double>> &z) {
        const double EPSILON = 1e-12;
        int n = A.size();

        for (int i = 0; i < n; i++) {
            // انتخاب Pivot
            int maxRow = i;
            for (int k = i + 1; k < n; k++) {
                if (abs(A[k][i]) > abs(A[maxRow][i])) {
                    maxRow = k;
                }
            }

            // سطرها رو جابجا کن
            if (maxRow != i) {
                swap(A[i], A[maxRow]);
                swap(z[i], z[maxRow]);
            }

            // جلوگیری از تقسیم بر صفر
            if (abs(A[i][i]) < EPSILON) {
                A[i][i] += EPSILON;
            }

            complex<double> diag = A[i][i];

            // نرمال‌سازی سطر
            for (int j = i; j < n; j++) {
                A[i][j] /= diag;
            }
            z[i] /= diag;

            // حذف گاوسی
            for (int k = 0; k < n; k++) {
                if (k != i && abs(A[k][i]) > EPSILON) {
                    complex<double> factor = A[k][i];
                    for (int j = i; j < n; j++) {
                        A[k][j] -= factor * A[i][j];
                    }
                    z[k] -= factor * z[i];
                }
            }
        }
        return z;
    }

    vector<double> solveLinearSystem2(vector<vector<double>> A, vector<double> &z) {
        const double EPSILON = 1e-12;
        int n = A.size();

        for (int i = 0; i < n; i++) {
            int maxRow = i;
            for (int k = i + 1; k < n; k++) {
                if (abs(A[k][i]) > abs(A[maxRow][i])) {
                    maxRow = k;
                }
            }
            swap(A[i], A[maxRow]);
            swap(z[i], z[maxRow]);

            if (abs(A[i][i]) < EPSILON) {
                A[i][i] += EPSILON;
            }

            double pivot = A[i][i];
            for (int j = i; j < n; j++) {
                A[i][j] /= pivot;
            }
            z[i] /= pivot;

            for (int k = i + 1; k < n; k++) {
                double factor = A[k][i];
                for (int j = i; j < n; j++) {
                    A[k][j] -= factor * A[i][j];
                }
                z[k] -= factor * z[i];
            }
        }

        // back substitution
        vector<double> x(n);
        for (int i = n - 1; i >= 0; i--) {
            x[i] = z[i];
            for (int j = i + 1; j < n; j++) {
                x[i] -= A[i][j] * x[j];
            }
        }

        return x;
    }

///-------
    string analyzeTransient(double tStart, double tEnd, double step,
                            vector<shared_ptr<Node>>& nodes,
                            vector<shared_ptr<Element>>& elements) {
        findError(elements,nodes);
        stringstream ss;
        ofstream outFile("data/log.txt");

        if (!outFile.is_open()) {
            cerr << "Unable to open log.txt for writing" << endl;
            return "Error: Could not open log file";
        }

// 1. شناسایی نودهای فعال
        set<string> activeNodeNames;
        for (const auto& elem : elements) {
            if (elem->getNodeP() && !elem->getNodeP()->getIsGround()) {
                activeNodeNames.insert(elem->getNodeP()->getName());
            }
            if (elem->getNodeN() && !elem->getNodeN()->getIsGround()) {
                activeNodeNames.insert(elem->getNodeN()->getName());
            }
// اضافه کردن نودهای کنترل‌کننده منابع وابسته
            if (auto vcvs = dynamic_pointer_cast<VCVS>(elem)) {
                if (vcvs->getControlNodeP() && !vcvs->getControlNodeP()->getIsGround())
                    activeNodeNames.insert(vcvs->getControlNodeP()->getName());
                if (vcvs->getControlNodeN() && !vcvs->getControlNodeN()->getIsGround())
                    activeNodeNames.insert(vcvs->getControlNodeN()->getName());
            } else if (auto vccs = dynamic_pointer_cast<VCCS>(elem)) {
                if (vccs->getControlNodeP() && !vccs->getControlNodeP()->getIsGround())
                    activeNodeNames.insert(vccs->getControlNodeP()->getName());
                if (vccs->getControlNodeN() && !vccs->getControlNodeN()->getIsGround())
                    activeNodeNames.insert(vccs->getControlNodeN()->getName());
            }
        }

// 2. ایجاد نقشه نام نود به شاخص
        map<string, int> nodeIndex;
        int idx = 0;
        for (set<string>::iterator it = activeNodeNames.begin(); it != activeNodeNames.end(); ++it) {
            nodeIndex[*it] = idx++;
        }

// 3. شمارش منابع ولتاژ و دیودها
        int n = idx;
        vector<shared_ptr<Element>> voltageSourcesAndDiodes;
        vector<shared_ptr<Diode>> diodes;

// ذخیره داده‌های خروجی
        map<string, vector<pair<double, double>>> voltageResults;
        map<string, vector<pair<double, double>>> currentResults;

// مقداردهی اولیه عناصر دینامیک و جداسازی منابع ولتاژ و دیودها
        for (const auto& elem : elements) {
            string type = elem->getType();
            if (type == "VoltageSource" || type == "VCVS" || type == "CCVS") {
                voltageSourcesAndDiodes.push_back(elem);
            } else if (type.rfind("Diode", 0) == 0) { // Check if type starts with "Diode"
                auto d = dynamic_pointer_cast<Diode>(elem);
                diodes.push_back(d);
                voltageSourcesAndDiodes.push_back(elem);
            } else if (auto cap = dynamic_pointer_cast<Capacitor>(elem)) {
                cap->setPreviousVoltage(0.0);
            } else if (auto ind = dynamic_pointer_cast<Inductor>(elem)) {
                ind->setPreviousCurrent(0.0);
            }
        }
        int m = voltageSourcesAndDiodes.size();

// 4. حلقه زمانی
        for (double t = 0; t <= tEnd + 1e-12; t += step) {
            bool converged = false;
            int iterationLimit = 50; // جلوگیری از حلقه بی‌نهایت
            int iteration = 0;
            vector<double> solution;

// در ابتدای هر گام زمانی، وضعیت دیودها را بر اساس ولتاژ قبلی حدس بزن
            for(size_t i = 0; i < diodes.size(); ++i) {
                diodes[i]->assumeState(diodes[i]->getVoltage() >= diodes[i]->getValue());
            }

            while (!converged && iteration < iterationLimit) {
// ماتریس‌های سیستم
                vector<vector<double>> G(n, vector<double>(n, 0.0));
                vector<vector<double>> B(n, vector<double>(m, 0.0));
                vector<vector<double>> D(m, vector<double>(n, 0.0));
                vector<vector<double>> E(m, vector<double>(m, 0.0));
                vector<double> I(n, 0.0);
                vector<double> F(m, 0.0);

// 5. ساخت ماتریس‌ها
                for (const auto& elem : elements) {
                    auto pn = elem->getNodeP();
                    auto nn = elem->getNodeN();
                    int pi = (pn && !pn->getIsGround()) ? nodeIndex.at(pn->getName()) : -1;
                    int ni = (nn && !nn->getIsGround()) ? nodeIndex.at(nn->getName()) : -1;
                    string type = elem->getType();

                    if (type == "Resistor") {
                        double g = 1.0 / elem->getValue();
                        if (pi != -1) G[pi][pi] += g;
                        if (ni != -1) G[ni][ni] += g;
                        if (pi != -1 && ni != -1) {
                            G[pi][ni] -= g;
                            G[ni][pi] -= g;
                        }
                    } else if (type == "Capacitor") {
                        auto cap = dynamic_pointer_cast<Capacitor>(elem);
                        double geq = cap->getValue() / step;
                        double ieq = geq * cap->getPreviousVoltage();
                        if (pi != -1) G[pi][pi] += geq;
                        if (ni != -1) G[ni][ni] += geq;
                        if (pi != -1 && ni != -1) { G[pi][ni] -= geq; G[ni][pi] -= geq; }
                        if (pi != -1) I[pi] += ieq;
                        if (ni != -1) I[ni] -= ieq;
                    } else if (type == "Inductor") {
                        auto ind = dynamic_pointer_cast<Inductor>(elem);
                        double g_eq = step / ind->getValue();
                        double ieq = ind->getPreviousCurrent();
                        if(pi != -1) G[pi][pi] += g_eq;
                        if(ni != -1) G[ni][ni] += g_eq;
                        if(pi !=-1 && ni != -1) { G[pi][ni] -= g_eq; G[ni][pi] -= g_eq; }
                        if(pi != -1) I[pi] -= ieq;
                        if(ni != -1) I[ni] += ieq;
                    } else if (type == "CurrentSource") {
                        dynamic_pointer_cast<CurrentSource>(elem)->setValueAtTime(t);
                        if (pi != -1) I[pi] -= elem->getValue();
                        if (ni != -1) I[ni] += elem->getValue();
                    } else if (type == "VCCS") {
                        auto vccs = dynamic_pointer_cast<VCCS>(elem);
                        double gain = vccs->getGain();
                        auto ctrlP_node = vccs->getControlNodeP();
                        auto ctrlN_node = vccs->getControlNodeN();
                        int ctrl_pi = (ctrlP_node && !ctrlP_node->getIsGround()) ? nodeIndex.at(ctrlP_node->getName()) : -1;
                        int ctrl_ni = (ctrlN_node && !ctrlN_node->getIsGround()) ? nodeIndex.at(ctrlN_node->getName()) : -1;
                        if (pi != -1) {
                            if (ctrl_pi != -1) G[pi][ctrl_pi] += gain;
                            if (ctrl_ni != -1) G[pi][ctrl_ni] -= gain;
                        }
                        if (ni != -1) {
                            if (ctrl_pi != -1) G[ni][ctrl_pi] -= gain;
                            if (ctrl_ni != -1) G[ni][ctrl_ni] += gain;
                        }
                    } else if (type == "CCCS") {
                        dynamic_pointer_cast<CCCS>(elem)->setValueAtTime(t);
                        if (pi != -1) I[pi] -= elem->getValue();
                        if (ni != -1) I[ni] += elem->getValue();
                    }
                }

// Handle voltage sources and diodes
                for (int k = 0; k < m; ++k) {
                    auto elem = voltageSourcesAndDiodes[k];
                    auto pn = elem->getNodeP();
                    auto nn = elem->getNodeN();
                    int pi = (pn && !pn->getIsGround()) ? nodeIndex.at(pn->getName()) : -1;
                    int ni = (nn && !nn->getIsGround()) ? nodeIndex.at(nn->getName()) : -1;
                    string type = elem->getType();

                    if (type == "VoltageSource") {
                        dynamic_pointer_cast<VoltageSource>(elem)->setValueAtTime(t);
                        if (pi != -1) { B[pi][k] = 1; D[k][pi] = 1; }
                        if (ni != -1) { B[ni][k] = -1; D[k][ni] = -1; }
                        F[k] = elem->getValue();
                    } else if (type == "VCVS") {
                        auto vcvs = dynamic_pointer_cast<VCVS>(elem);
                        double gain = vcvs->getGain();
                        auto ctrlP_node = vcvs->getControlNodeP();
                        auto ctrlN_node = vcvs->getControlNodeN();
                        int ctrl_pi = (ctrlP_node && !ctrlP_node->getIsGround()) ? nodeIndex.at(ctrlP_node->getName()) : -1;
                        int ctrl_ni = (ctrlN_node && !ctrlN_node->getIsGround()) ? nodeIndex.at(ctrlN_node->getName()) : -1;
                        if (pi != -1) { B[pi][k] = 1; D[k][pi] = 1; }
                        if (ni != -1) { B[ni][k] = -1; D[k][ni] = -1; }
                        if (ctrl_pi != -1) D[k][ctrl_pi] -= gain;
                        if (ctrl_ni != -1) D[k][ctrl_ni] += gain;
                        F[k] = 0.0;
                    } else if (type == "CCVS") {
                        dynamic_pointer_cast<CCVS>(elem)->setValueAtTime(t);
                        if (pi != -1) { B[pi][k] = 1; D[k][pi] = 1; }
                        if (ni != -1) { B[ni][k] = -1; D[k][ni] = -1; }
                        F[k] = elem->getValue();
                    } else if (type.rfind("Diode", 0) == 0) {
                        auto d = dynamic_pointer_cast<Diode>(elem);
                        if (d->isAssumedOn()) { // Model as Voltage Source vOn
                            if (pi != -1) { B[pi][k] = 1; D[k][pi] = 1; }
                            if (ni != -1) { B[ni][k] = -1; D[k][ni] = -1; }
                            F[k] = d->getValue(); // vOn
                        } else { // Model as Open Circuit (0A current source)
                            E[k][k] = 1.0;
                            F[k] = 0.0;
                        }
                    }
                }

// 6. حل سیستم
                vector<vector<double>> A(n + m, vector<double>(n + m, 0.0));
                vector<double> Z(n + m, 0.0);
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j < n; j++) A[i][j] = G[i][j];
                    for (int j = 0; j < m; j++) A[i][n + j] = B[i][j];
                    Z[i] = I[i];
                }
                for (int i = 0; i < m; i++) {
                    for (int j = 0; j < n; j++) A[n + i][j] = D[i][j];
                    for (int j = 0; j < m; j++) A[n + i][n + j] = E[i][j];
                    Z[n + i] = F[i];
                }

                solution = solveLinearSystem(A, Z);

// 7. بررسی همگرایی دیودها
                converged = true;
                for (size_t i = 0; i < diodes.size(); ++i) {
                    shared_ptr<Diode> d = diodes[i];
                    auto pn = d->getNodeP();
                    auto nn = d->getNodeN();
                    double p_volt = (pn && !pn->getIsGround()) ? solution[nodeIndex.at(pn->getName())] : 0.0;
                    double n_volt = (nn && !nn->getIsGround()) ? solution[nodeIndex.at(nn->getName())] : 0.0;
                    double diode_voltage = p_volt - n_volt;

                    ptrdiff_t vs_idx = -1;
                    for(size_t k=0; k<voltageSourcesAndDiodes.size(); ++k){
                        if(voltageSourcesAndDiodes[k] == d){
                            vs_idx = k;
                            break;
                        }
                    }
                    double diode_current = solution[n + vs_idx];

                    bool old_assumption = d->isAssumedOn();
                    bool new_state;

                    if (old_assumption) { // Was ON
                        new_state = (diode_current >= 0); // Must have positive current to stay ON
                    } else { // Was OFF
                        new_state = (diode_voltage >= d->getValue()); // Must have enough voltage to turn ON
                    }

                    if (old_assumption != new_state) {
                        converged = false;
                        d->assumeState(new_state); // Update assumption for next iteration
                    }
                }
                iteration++;
            } // End of convergence loop

// 8. به‌روزرسانی ولتاژ نودها و وضعیت نهایی عناصر
            for (map<string, int>::iterator it = nodeIndex.begin(); it != nodeIndex.end(); ++it) {
                string name = it->first;
                int index = it->second;
                double voltage = solution[index];

                auto sameNameNodes = Wire::findNodeWhitname(name);
                for (size_t i = 0; i < sameNameNodes.size(); i++) {
                    sameNameNodes[i]->setVoltage(voltage);
                }

                if (t >= tStart) {
                    voltageResults["V(" + name + ")"].push_back({t, voltage});
                }
            }

// به‌روزرسانی جریان و حالت عناصر
            for (const auto& elem : elements) {
                double current = 0.0;
                string type = elem->getType();
                if (type == "Capacitor") {
                    auto cap = dynamic_pointer_cast<Capacitor>(elem);
                    double v = cap->getVoltage();
                    current = (cap->getValue() / step) * (v - cap->getPreviousVoltage());
                    cap->setCurrent(current);
                    cap->setPreviousVoltage(v);
                } else if (type == "Inductor") {
                    auto ind = dynamic_pointer_cast<Inductor>(elem);
                    double v = ind->getVoltage();
                    current = ind->getPreviousCurrent() + (step / ind->getValue()) * v;
                    ind->setCurrent(current);
                    ind->setPreviousCurrent(current);
                } else if (type == "Resistor") {
                    current = elem->getVoltage() / elem->getValue();
                    elem->setCurrent(current);
                } else if (type == "CurrentSource" || type == "VCCS" || type == "CCCS") {
                    if (auto vccs = dynamic_pointer_cast<VCCS>(elem)) vccs->setValueAtTime(t);
                    else if (auto cccs = dynamic_pointer_cast<CCCS>(elem)) cccs->setValueAtTime(t);
                    current = elem->getValue();
                    elem->setCurrent(current);
                }
            }

// به‌روزرسانی جریان منابع ولتاژ و دیودها
            for(size_t k=0; k<voltageSourcesAndDiodes.size(); ++k) {
                auto elem = voltageSourcesAndDiodes[k];
                elem->setCurrent(solution[n+k]);
                if (elem->getType().rfind("Diode", 0) == 0) {
                    dynamic_pointer_cast<Diode>(elem)->updateState();
                }
            }

// ذخیره جریان‌ها
            if (t >= tStart) {
                for (const auto& elem : elements) {
                    currentResults["I(" + elem->getName() + ")"].push_back({t, elem->getCurrent()});
                }
            }

        } // End of time loop

// 9. نوشتن نتایج در خروجی
        for (const auto& entry : voltageResults) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << point.first << "," << point.second << endl;
                ss << point.first << "," << point.second << endl;
            }
        }
        for (const auto& entry : currentResults) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << point.first << "," << point.second << endl;
                ss << point.first << "," << point.second << endl;
            }
        }

        outFile.close();
        return "Tran Good";
    }


// تابع کمکی برای نوشتن داده‌ها
    void writeDataToFile(ofstream& outFile, stringstream& ss,
                         map<string, vector<tuple<double, double>>>& voltageAmplitudes,
                         map<string, vector<tuple<double, double>>>& voltagePhases,
                         map<string, vector<tuple<double, double>>>& currentAmplitudes,
                         map<string, vector<tuple<double, double>>>& currentPhases) {

        for (const auto& entry : voltageAmplitudes) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

        for (const auto& entry : voltagePhases) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

        for (const auto& entry : currentAmplitudes) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

        for (const auto& entry : currentPhases) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }
    }

    string analyzeAc(string type, string tStart, string tEnd, string numberOfPoints,
                     int componentId,
                     vector<shared_ptr<Element>>& elements, vector<shared_ptr<LabelNet>>& labels) {

        if (elements.empty()) return "empty";
        findError(elements, Wire::allNodes);

        double TStart = 0;
        double TStop = 0;
        double Step = 0;

        if (tStart != "0")
            TStart = unitHandler3(tStart, "start time");
        TStop = unitHandler3(tEnd, "stop time");
        Step = unitHandler3(numberOfPoints, "time steps");
        Step = (TStop - TStart) / Step;

        stringstream ss;
        ofstream clearFile("data/log.txt", ios::trunc);
        clearFile.close();

        ofstream outFile("data/log.txt", ios::app);
        if (!outFile.is_open()) {
            cerr << "Unable to open log.txt for writing" << endl;
            return "Error: Could not open log file";
        }

// مقداردهی اولیه مقادیر AC برای منابع ولتاژ مستقل
        for (auto &i : elements) {
            if (i->getType() == "VoltageSource") {
                i->AcValue = {i->getValue() * cos(i->phase), i->getValue() * sin(i->phase)};
                i->AcVoltage = {i->getValue() * cos(i->phase), i->getValue() * sin(i->phase)};
            }
        }

// ایجاد نگاشت از نام گره به اندیس ماتریس
        map<string, int> nodeIndex;
        int idx = 0;
        for (int i = 1; i <= componentId; ++i) {
            string nodeName = "N0" + to_string(i);
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
                    nodeIndex[n->getName()] = idx++;
                }
            }
        }

        for (auto &label : labels) {
            string nodeName = label->text;
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
                    if (nodeIndex.find(n->getName()) == nodeIndex.end()) {
                        nodeIndex[n->getName()] = idx++;
                    }
                }
            }
        }

        int n = idx;

// شمارش تمام انواع منابع ولتاژ و ایجاد نقشه اندیس
        int m = 0;
        vector<shared_ptr<VoltageSource>> voltageSources;
        map<string, int> voltageSourceNameToIndex;
        for (shared_ptr<Element>& e : elements) {
            if (auto vs = dynamic_pointer_cast<VoltageSource>(e)) {
                voltageSources.push_back(vs);
                voltageSourceNameToIndex[e->getName()] = m;
                m++;
            }
        }
        int matrixSize = n + m;

        map<string, vector<tuple<double, double>>> voltageAmplitudes;
        map<string, vector<tuple<double, double>>> voltagePhases;
        map<string, vector<tuple<double, double>>> currentAmplitudes;
        map<string, vector<tuple<double, double>>> currentPhases;

        int checkForWrite = 0;
        const int WRITE_THRESHOLD = 500;

        for (double frc = TStart; frc <= TStop + 1e-12; frc += Step) {
            vector<vector<complex<double>>> G(n, vector<complex<double>>(n, 0.0));
            vector<vector<complex<double>>> B(n, vector<complex<double>>(m, 0.0));
            vector<vector<complex<double>>> C(m, vector<complex<double>>(n, 0.0));
            vector<vector<complex<double>>> D(m, vector<complex<double>>(m, 0.0));
            vector<complex<double>> I(n, 0.0);
            vector<complex<double>> F(m, 0.0);

            int vsIndex = 0;
            for (shared_ptr<Element>& e : elements) {
                string type = e->getType();
                shared_ptr<Node> pn = e->getNodeP();
                shared_ptr<Node> nn = e->getNodeN();
                int pi = (pn && !pn->getIsGround()) ? nodeIndex[pn->getName()] : -1;
                int ni = (nn && !nn->getIsGround()) ? nodeIndex[nn->getName()] : -1;

                if (type == "Resistor" || type == "Inductor" || type == "Capacitor") {
                    complex<double> r;
                    if (type == "Resistor") {
                        r = e->getValue();
                    }
                    if (type == "Inductor") {
                        r = frc * e->getValue() * complex<double>(0.0, 1.0);
                    }
                    if (type == "Capacitor") {
                        if (frc == 0) r = 1e12;
                        else r = 1.0 / (frc * e->getValue() * complex<double>(0.0, 1.0));
                    }

                    complex<double> g = (abs(r) < 1e-12) ? 1e12 : 1.0 / r;
                    if (pi != -1) G[pi][pi] += g;
                    if (ni != -1) G[ni][ni] += g;
                    if (pi != -1 && ni != -1) {
                        G[pi][ni] -= g;
                        G[ni][pi] -= g;
                    }
                }
                else if (type == "CurrentSource") {
                    double prevI = e->getValue();
                    if (pi != -1) I[pi] -= prevI;
                    if (ni != -1) I[ni] += prevI;
                }
                else if (type == "VCCS") {
                    auto vccs = dynamic_pointer_cast<VCCS>(e);
                    double g = vccs->getGain();
                    auto cpn = vccs->getControlNodeP();
                    auto cnn = vccs->getControlNodeN();
                    int cpi = (cpn && !cpn->getIsGround()) ? nodeIndex.at(cpn->getName()) : -1;
                    int cni = (cnn && !cnn->getIsGround()) ? nodeIndex.at(cnn->getName()) : -1;

                    if (pi != -1) {
                        if (cpi != -1) G[pi][cpi] += g;
                        if (cni != -1) G[pi][cni] -= g;
                    }
                    if (ni != -1) {
                        if (cpi != -1) G[ni][cpi] -= g;
                        if (cni != -1) G[ni][cni] += g;
                    }
                }
                else if (type == "CCCS") {
                    auto cccs = dynamic_pointer_cast<CCCS>(e);
                    double beta = cccs->getGain();
                    auto ctrlElement = cccs->getControlSourceName();
                    if (ctrlElement && dynamic_pointer_cast<VoltageSource>(ctrlElement)) {
                        int ctrlVsIndex = voltageSourceNameToIndex.at(ctrlElement->getName());
                        if (pi != -1) B[pi][ctrlVsIndex] += beta;
                        if (ni != -1) B[ni][ctrlVsIndex] -= beta;
                    }
                }
                else if (dynamic_pointer_cast<VoltageSource>(e)) {
                    if (pi != -1) {
                        B[pi][vsIndex] = 1.0;
                        C[vsIndex][pi] = 1.0;
                    }
                    if (ni != -1) {
                        B[ni][vsIndex] = -1.0;
                        C[vsIndex][ni] = -1.0;
                    }

                    if (type == "VCVS") {
                        auto vcvs = dynamic_pointer_cast<VCVS>(e);
                        double mu = vcvs->getGain();
                        auto cpn = vcvs->getControlNodeP();
                        auto cnn = vcvs->getControlNodeN();
                        int cpi = (cpn && !cpn->getIsGround()) ? nodeIndex.at(cpn->getName()) : -1;
                        int cni = (cnn && !cnn->getIsGround()) ? nodeIndex.at(cnn->getName()) : -1;

                        if (cpi != -1) C[vsIndex][cpi] -= mu;
                        if (cni != -1) C[vsIndex][cni] += mu;
                        F[vsIndex] = 0.0;
                    }
                    else if (type == "CCVS") {
                        auto ccvs = dynamic_pointer_cast<CCVS>(e);
                        double r = ccvs->getGain();
                        auto ctrlElement = ccvs->getControlSourceName();
                        if (ctrlElement && dynamic_pointer_cast<VoltageSource>(ctrlElement)) {
                            int ctrlVsIndex = voltageSourceNameToIndex.at(ctrlElement->getName());
                            D[vsIndex][ctrlVsIndex] = -r;
                            F[vsIndex] = 0.0;
                        }
                    }
                    else {
                        F[vsIndex] = e->AcVoltage;
                    }
                    vsIndex++;
                }
            }

// ساخت ماتریس A و بردار Z
            vector<vector<complex<double>>> A(matrixSize, vector<complex<double>>(matrixSize, 0.0));
            vector<complex<double>> Z(matrixSize, 0.0);

            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) A[i][j] = G[i][j];
                for (int j = 0; j < m; j++) A[i][n + j] = B[i][j];
                Z[i] = I[i];
            }
            for (int i = 0; i < m; i++) {
                for (int j = 0; j < n; j++) A[n + i][j] = C[i][j];
                for (int j = 0; j < m; j++) A[n + i][n + j] = D[i][j];
                Z[n + i] = F[i];
            }

            vector<complex<double>> solution = solveLinearSystemComplex(A, Z);

// استخراج نتایج
            for (shared_ptr<Node>& node : Wire::allNodes) {
                if (!node) continue;

                auto it = nodeIndex.find(node->getName());
                if (it != nodeIndex.end()) {
                    int nodeIdx = it->second;

                    if (node->getIsGround()) {
                        node->AcVoltage = {0.0, 0.0};
                    }
                    else if (nodeIdx >= 0 && nodeIdx < solution.size()) {
                        node->AcVoltage = solution[nodeIdx];
                        if (type == "dec") {
                            voltageAmplitudes["V(" + node->name + ")(amplitude)"].push_back(
                                    make_tuple(frc, abs(solution[nodeIdx]))
                            );
                        }
                        if (type == "log") {
                            voltageAmplitudes["V(" + node->name + ")(amplitude)"].push_back(
                                    make_tuple(frc, 20 * log10(abs(solution[nodeIdx])))
                            );
                        }

                        voltagePhases["V(" + node->name + ")(phase)"].push_back(
                                make_tuple(frc, arg(solution[nodeIdx]))
                        );
                    }
                    else {
                        node->AcVoltage = {0.0, 0.0};
                    }
                }
            }

            for (int i = 0; i < m; i++) {
                voltageSources[i]->AcCurrent = (solution[n + i]);

                if (type == "dec") {
                    currentAmplitudes["I(" + voltageSources[i]->getName() + ")(amplitude)"].push_back(
                            make_tuple(frc, abs(solution[n + i]))
                    );
                }
                if (type == "log") {
                    currentAmplitudes["I(" + voltageSources[i]->getName() + ")(amplitude)"].push_back(
                            make_tuple(frc, 20 * log10(abs(solution[n + i])))
                    );
                }

                currentPhases["I(" + voltageSources[i]->getName() + ")(phase)"].push_back(
                        make_tuple(frc, arg(solution[n + i]))
                );
            }

            for (shared_ptr<Element>& e : elements) {
                e->AcVoltage = e->getNodeP()->AcVoltage - e->getNodeN()->AcVoltage;

                if (e->getType() == "Resistor") {
                    e->AcCurrent = (e->getValue() == 0) ? e->AcVoltage * 1e12 : e->AcVoltage / e->getValue();
                }
                if (e->getType() == "Inductor") {
                    if (frc == 0 || e->getValue() == 0) e->AcCurrent = {0.0, 0.0};
                    else e->AcCurrent = e->AcVoltage / (frc * e->getValue() * complex<double>(0.0, 1.0));
                }
                if (e->getType() == "Capacitor") {
                    e->AcCurrent = e->AcVoltage * (frc * e->getValue() * complex<double>(0.0, 1.0));
                }

                if (dynamic_pointer_cast<Resistor>(e) || dynamic_pointer_cast<Inductor>(e) || dynamic_pointer_cast<Capacitor>(e)) {
                    if (type == "dec") {
                        currentAmplitudes["I(" + e->getName() + ")(amplitude)"].push_back(
                                make_tuple(frc, abs(e->AcCurrent))
                        );
                    }
                    if (type == "log") {
                        currentAmplitudes["I(" + e->getName() + ")(amplitude)"].push_back(
                                make_tuple(frc, 20 * log10(abs(e->AcCurrent)))
                        );
                    }

                    currentPhases["I(" + e->getName() + ")(phase)"].push_back(
                            make_tuple(frc, arg(e->AcCurrent))
                    );
                }
            }

            checkForWrite++;
            if (checkForWrite >= WRITE_THRESHOLD) {
// نوشتن داده‌ها در فایل
                writeDataToFile(outFile, ss, voltageAmplitudes, voltagePhases, currentAmplitudes, currentPhases);

// پاک کردن مپ‌ها
                voltageAmplitudes.clear();
                voltagePhases.clear();
                currentAmplitudes.clear();
                currentPhases.clear();

                checkForWrite = 0;
                outFile.flush();
            }
        }

// نوشتن داده‌های باقیمانده
        if (!voltageAmplitudes.empty() || !currentAmplitudes.empty()) {
            writeDataToFile(outFile, ss, voltageAmplitudes, voltagePhases, currentAmplitudes, currentPhases);
        }

        outFile.close();
        return "ok";
    }


    string analyzePhase(string tStart, string tEnd, string numberOfPoints,
                        int componentId, string Bfrc,
                        vector<shared_ptr<Element>>& elements,vector<shared_ptr<LabelNet>>&labels) {
        if(elements.empty())return "empty";
        findError(elements, Wire::allNodes);
        double TStart = 0;
        double TStop = 0;
        double Step = 0;
        if (tStart != "0")
            TStart = unitHandler3(tStart, "start time");
        TStop = unitHandler3(tEnd, "stop time");
        Step = unitHandler3(numberOfPoints, "time steps");
        Step = (TStop - TStart) / Step;
        double frc = unitHandler3(Bfrc, "frequency");

        stringstream ss;
        ofstream outFile("data/log.txt");

        if (!outFile.is_open()) {
            cerr << "Unable to open logPhase.txt for writing" << endl;
            return "Error: Could not open log file";
        }

// مقداردهی اولیه مقادیر AC برای منابع ولتاژ مستقل
        for (auto &i : elements) {
            if (i->getType() == "VoltageSource") {
                i->AcValue = {i->getValue() * cos(i->phase), i->getValue() * sin(i->phase)};
                i->AcVoltage = {i->getValue() * cos(i->phase), i->getValue() * sin(i->phase)};
            }
        }

// ایجاد نگاشت از نام گره به اندیس ماتریس
        map<string, int> nodeIndex;
        int idx = 0;
        for (int i = 1; i <= componentId; ++i) {
            string nodeName = "N0" + to_string(i);
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
                    nodeIndex[n->getName()] = idx++;
                }
            }
        }
        for (auto &label : labels) {
            string nodeName = label->text;
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
// فقط اگر قبلاً اضافه نشده باشد
                    if (nodeIndex.find(n->getName()) == nodeIndex.end()) {
                        nodeIndex[n->getName()] = idx++;
                    }
                }
            }
        }

        int n = idx;

// شمارش تمام انواع منابع ولتاژ (مستقل و وابسته) و ایجاد نقشه اندیس آن‌ها
        int m = 0;
        vector<shared_ptr<VoltageSource>> voltageSources;
        map<string, int> voltageSourceNameToIndex;
        for (shared_ptr<Element>& e : elements) {
            if (auto vs = dynamic_pointer_cast<VoltageSource>(e)) {
                voltageSources.push_back(vs);
                voltageSourceNameToIndex[e->getName()] = m;
                m++;
            }
        }
        int matrixSize = n + m;

// ایجاد نگاشت‌های جداگانه برای دامنه و فاز
        map<string, vector<tuple<double, double>>> voltageAmplitudes;
        map<string, vector<tuple<double, double>>> voltagePhases;
        map<string, vector<tuple<double, double>>> currentAmplitudes;
        map<string, vector<tuple<double, double>>> currentPhases;

        for (double phase = TStart; phase <= TStop + 1e-12; phase += Step) {
            vector<vector<complex<double>>> G(n, vector<complex<double>>(n, 0.0));
            vector<vector<complex<double>>> B(n, vector<complex<double>>(m, 0.0));
            vector<vector<complex<double>>> C(m, vector<complex<double>>(n, 0.0));
            vector<vector<complex<double>>> D(m, vector<complex<double>>(m, 0.0));
            vector<complex<double>> I(n, 0.0);
            vector<complex<double>> F(m, 0.0);

            int vsIndex = 0;
            for (shared_ptr<Element>& e : elements) {
                string type = e->getType();
                shared_ptr<Node> pn = e->getNodeP();
                shared_ptr<Node> nn = e->getNodeN();
                int pi = (pn && !pn->getIsGround()) ? nodeIndex[pn->getName()] : -1;
                int ni = (nn && !nn->getIsGround()) ? nodeIndex[nn->getName()] : -1;

                if (type == "Resistor" || type == "Inductor" || type == "Capacitor") {
                    complex<double> r;
                    if ((type == "Resistor")) {
                        r = e->getValue();
                    }
                    if ((type == "Inductor")) {
                        r = frc * e->getValue() * complex<double>(0.0, 1.0);
                    }
                    if ((type == "Capacitor")) {
                        r = 1.0 / (frc * e->getValue() * complex<double>(0.0, 1.0));
                    }

                    complex<double> g = (abs(r) < 1e-12) ? 1e12 : 1.0 / r;
                    if (pi != -1) G[pi][pi] += g;
                    if (ni != -1) G[ni][ni] += g;
                    if (pi != -1 && ni != -1) {
                        G[pi][ni] -= g;
                        G[ni][pi] -= g;
                    }
                }
                else if (type == "CurrentSource") {
// به‌روزرسانی فاز منبع جریان
                    complex<double> currentValue = e->getValue() * polar(1.0, phase);
                    if (pi != -1) I[pi] -= currentValue;
                    if (ni != -1) I[ni] += currentValue;
                }
                else if (type == "VCCS") {
                    auto vccs = dynamic_pointer_cast<VCCS>(e);
                    double g = vccs->getGain();
                    auto cpn = vccs->getControlNodeP();
                    auto cnn = vccs->getControlNodeN();
                    int cpi = (cpn && !cpn->getIsGround()) ? nodeIndex.at(cpn->getName()) : -1;
                    int cni = (cnn && !cnn->getIsGround()) ? nodeIndex.at(cnn->getName()) : -1;

                    if (pi != -1) {
                        if (cpi != -1) G[pi][cpi] += g;
                        if (cni != -1) G[pi][cni] -= g;
                    }
                    if (ni != -1) {
                        if (cpi != -1) G[ni][cpi] -= g;
                        if (cni != -1) G[ni][cni] += g;
                    }
                }
                else if (type == "CCCS") {
                    auto cccs = dynamic_pointer_cast<CCCS>(e);
                    double beta = cccs->getGain();
                    auto ctrlElement = cccs->getControlSourceName();
                    if (ctrlElement && dynamic_pointer_cast<VoltageSource>(ctrlElement)) {
                        int ctrlVsIndex = voltageSourceNameToIndex.at(ctrlElement->getName());
                        if (pi != -1) B[pi][ctrlVsIndex] += beta;
                        if (ni != -1) B[ni][ctrlVsIndex] -= beta;
                    }
                }
                else if (dynamic_pointer_cast<VoltageSource>(e)) {
// به‌روزرسانی فاز منبع ولتاژ
                    if (pi != -1) {
                        B[pi][vsIndex] = 1.0;
                        C[vsIndex][pi] = 1.0;
                    }
                    if (ni != -1) {
                        B[ni][vsIndex] = -1.0;
                        C[vsIndex][ni] = -1.0;
                    }

                    if (type == "VCVS") {
                        auto vcvs = dynamic_pointer_cast<VCVS>(e);
                        double mu = vcvs->getGain();
                        auto cpn = vcvs->getControlNodeP();
                        auto cnn = vcvs->getControlNodeN();
                        int cpi = (cpn && !cpn->getIsGround()) ? nodeIndex.at(cpn->getName()) : -1;
                        int cni = (cnn && !cnn->getIsGround()) ? nodeIndex.at(cnn->getName()) : -1;

                        if (cpi != -1) C[vsIndex][cpi] -= mu;
                        if (cni != -1) C[vsIndex][cni] += mu;
                        F[vsIndex] = 0.0;
                    }
                    else if (type == "CCVS") {
                        auto ccvs = dynamic_pointer_cast<CCVS>(e);
                        double r = ccvs->getGain();
                        auto ctrlElement = ccvs->getControlSourceName();
                        if (ctrlElement && dynamic_pointer_cast<VoltageSource>(ctrlElement)) {
                            int ctrlVsIndex = voltageSourceNameToIndex.at(ctrlElement->getName());
                            D[vsIndex][ctrlVsIndex] = -r;
                            F[vsIndex] = 0.0;
                        }
                    }
                    else {
// منبع ولتاژ مستقل با فاز متغیر
                        F[vsIndex] = e->getValue() * polar(1.0, phase);
                    }
                    vsIndex++;
                }
            }

            vector<vector<complex<double>>> A(matrixSize, vector<complex<double>>(matrixSize, 0.0));
            vector<complex<double>> Z(matrixSize, 0.0);

            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) A[i][j] = G[i][j];
                for (int j = 0; j < m; j++) A[i][n + j] = B[i][j];
                Z[i] = I[i];
            }
            for (int i = 0; i < m; i++) {
                for (int j = 0; j < n; j++) A[n + i][j] = C[i][j];
                for (int j = 0; j < m; j++) A[n + i][n + j] = D[i][j];
                Z[n + i] = F[i];
            }

            vector<complex<double>> solution = solveLinearSystemComplex(A, Z);

// ذخیره نتایج ولتاژ گره‌ها
            for (shared_ptr<Node>& node : Wire::allNodes) {
                if (!node) continue;

                auto it = nodeIndex.find(node->getName());
                if (it != nodeIndex.end()) {
                    int nodeIdx = it->second;

                    if (node->getIsGround()) {
                        node->AcVoltage = {0.0, 0.0};
                    }
                    else if (nodeIdx >= 0 && nodeIdx < solution.size()) {
                        node->AcVoltage = solution[nodeIdx];
                        voltageAmplitudes["V(" + node->name + ")(amplitude)"].push_back(
                                make_tuple(phase, abs(solution[nodeIdx]))
                        );
                        voltagePhases["V(" + node->name + ")(phase)"].push_back(
                                make_tuple(phase, arg(solution[nodeIdx]))
                        );
                    }
                    else {
                        node->AcVoltage = {0.0, 0.0};
                    }
                }
            }

// ذخیره نتایج جریان منابع ولتاژ
            for (int i = 0; i < m; i++) {
                voltageSources[i]->AcCurrent = (solution[n + i]);
                currentAmplitudes["I(" + voltageSources[i]->getName() + ")(amplitude)"].push_back(
                        make_tuple(phase, abs(solution[n + i]))
                );
                currentPhases["I(" + voltageSources[i]->getName() + ")(phase)"].push_back(
                        make_tuple(phase, arg(solution[n + i]))
                );
            }

// محاسبه و ذخیره نتایج جریان عناصر پسیو
            for (shared_ptr<Element>& e : elements) {
                e->AcVoltage = e->getNodeP()->AcVoltage - e->getNodeN()->AcVoltage;
                if (e->getType() == "Resistor") {
                    e->AcCurrent = (e->getValue() == 0) ? e->AcVoltage * 1e12 : e->AcVoltage / e->getValue();
                }
                if (e->getType() == "Inductor") {
                    e->AcCurrent = e->AcVoltage / (frc * e->getValue() * complex<double>(0.0, 1.0));
                }
                if (e->getType() == "Capacitor") {
                    e->AcCurrent = e->AcVoltage * (frc * e->getValue() * complex<double>(0.0, 1.0));
                }
                if(dynamic_pointer_cast<Resistor>(e) || dynamic_pointer_cast<Inductor>(e) || dynamic_pointer_cast<Capacitor>(e)){
                    currentAmplitudes["I(" + e->getName() + ")(amplitude)"].push_back(
                            make_tuple(phase, abs(e->AcCurrent))
                    );
                    currentPhases["I(" + e->getName() + ")(phase)"].push_back(
                            make_tuple(phase, arg(e->AcCurrent))
                    );
                }
            }
        }

// نوشتن نتایج در فایل و رشته خروجی
// Write voltage amplitudes
        for (const auto& entry : voltageAmplitudes) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

// Write voltage phases
        for (const auto& entry : voltagePhases) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

// Write current amplitudes
        for (const auto& entry : currentAmplitudes) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

// Write current phases
        for (const auto& entry : currentPhases) {
            outFile << entry.first << endl;
            ss << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << get<0>(point) << "," << get<1>(point) << endl;
                ss << get<0>(point) << "," << get<1>(point) << endl;
            }
        }

        return "ok";
    }

    bool Truestate(vector<shared_ptr<Element>>& elements) {
        double V_threshold = 0.0;
        for (auto &e: elements) {
            if (e->getType() == "DiodeD" || e->getType() == "DiodeZ") {
                if (e->getType() == "DiodeZ") {
                    V_threshold = 0.7;
                }
                auto d = dynamic_pointer_cast<Diode>(e);
                if (!d) continue;

                double vdiode = d->getNodeP()->getVoltage() - d->getNodeN()->getVoltage();

                if (d->isAssumedOn()) {

                    if (d->getCurrent() < 0.0) {
                        return false;
                    }
                } else {
                    if (vdiode > V_threshold) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    string op(int componentId, vector<shared_ptr<Element>>&elements,vector<shared_ptr<LabelNet>>&labels) {

        map<string, int> nodeIndex;
        int idx = 0;
        for (int i = 1; i <= componentId; ++i) {
            string nodeName = "N0" + to_string(i);
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
                    nodeIndex[n->getName()] = idx++;
                }
            }
        }
        for (auto &label : labels) {
            string nodeName = label->text;
            auto nodes = Wire::findNodeWhitname(nodeName);
            if (!nodes.empty()) {
                shared_ptr<Node> n = nodes[0];
                if (n && !n->getIsGround()) {
// فقط اگر قبلاً اضافه نشده باشد
                    if (nodeIndex.find(n->getName()) == nodeIndex.end()) {
                        nodeIndex[n->getName()] = idx++;
                    }
                }
            }
        }

        int n = idx;
        int m = 0;
        vector<shared_ptr<VoltageSource>> voltageSources;
        vector<shared_ptr<Diode>> onDiodes;

        for (auto &e: elements) {
            if (e->getType() == "VoltageSource" || e->getType() == "VCVS" || e->getType() == "VCCS") {
                m++;
                voltageSources.push_back(dynamic_pointer_cast<VoltageSource>(e));
            }
            else if (e->type == "DiodeD" || e->type == "DiodeZ") {
                shared_ptr<Diode>d = dynamic_pointer_cast<Diode>(e);
                if (d && d->isAssumedOn()) {
                    onDiodes.push_back(d);
                    m++;
                }
            }
        }
        int matrixSize = n + m;
        vector<vector<double>> G(n, vector<double>(n, 0.0));
        vector<vector<double>> B(n, vector<double>(m, 0.0));
        vector<vector<double>> C(m, vector<double>(n, 0.0));
        vector<vector<double>> D(m, vector<double>(m, 0.0));
        vector<double> I(n, 0.0);
        vector<double> F(m, 0.0);
        int vsIndex = 0;
        for (auto &e: elements) {
            string type = e->getType();
            auto pn = e->getNodeP();
            auto nn = e->getNodeN();
            int pi = (!pn->getIsGround()) ? nodeIndex[pn->getName()] : -1;
            int ni = (!nn->getIsGround()) ? nodeIndex[nn->getName()] : -1;
            if (type == "Resistor" || type == "Inductor") {
                double r = (type == "Resistor") ? e->getValue() : 0.0;
                double g = (r == 0.0) ? 1e12 : 1.0 / r;
                if (pi != -1) G[pi][pi] += g;
                if (ni != -1) G[ni][ni] += g;
                if (pi != -1 && ni != -1) {
                    G[pi][ni] -= g;
                    G[ni][pi] -= g;
                }
            }
            else if (type == "CurrentSource") {
                double prevI = e->getValue();
                if (pi != -1) I[pi] += prevI;
                if (ni != -1) I[ni] -= prevI;
            }
            else if (type == "VoltageSource" || type == "VCVS" || type == "CCVS") {
                if (pi != -1) B[pi][vsIndex] = 1;
                if (ni != -1) B[ni][vsIndex] = -1;
                if (pi != -1) C[vsIndex][pi] = 1;
                if (ni != -1) C[vsIndex][ni] = -1;
                if (type == "VoltageSource") {
                    F[vsIndex] = e->getValue();
                }
                if (type == "VCVS") {
                    shared_ptr<VCVS>ee = dynamic_pointer_cast<VCVS>(e);
                    auto cp = ee->getControlNodeP();
                    auto cn = ee->getControlNodeN();
                    int cpi = (!cp->getIsGround()) ? nodeIndex[cp->getName()] : -1;
                    int cni = (!cn->getIsGround()) ? nodeIndex[cn->getName()] : -1;
                    double gain = ee->getGain();
                    if (cpi != -1) C[vsIndex][cpi] -= gain;
                    if (cni != -1) C[vsIndex][cni] += gain;
                    F[vsIndex] = 0.0;
                }
                else if (type == "CCVS") {
                    auto ee = dynamic_pointer_cast<CCVS>(e);
                    string ctrl = ee->getControlSourceName()->getName();
                    int ctrlIdx = -1;
                    for (int k = 0; k < voltageSources.size(); ++k) {
                        if (voltageSources[k]->getName() == ctrl) {
                            ctrlIdx = k;
                            break;
                        }
                    }
                    if (ctrlIdx != -1) {
                        D[vsIndex][ctrlIdx] = -ee->getGain();
                    }
                    F[vsIndex] = 0.0;
                }
                vsIndex++;
            }
            else if (e->getType() == "VCCS") {
                auto ee = dynamic_pointer_cast<VCCS>(e);
                auto cp = ee->getControlNodeP();
                auto cn = ee->getControlNodeN();
                int cpi = (!cp->getIsGround()) ? nodeIndex[cp->getName()] : -1;
                int cni = (!cn->getIsGround()) ? nodeIndex[cn->getName()] : -1;
                double g = ee->getGain();
                if (pi != -1 && cpi != -1) G[pi][cpi] += g;
                if (pi != -1 && cni != -1) G[pi][cni] -= g;
                if (ni != -1 && cpi != -1) G[ni][cpi] -= g;
                if (ni != -1 && cni != -1) G[ni][cni] += g;
            }
            else if (e->getType() == "CCCS") {
                auto ee = dynamic_pointer_cast<CCCS>(e);
                string ctrl = ee->getControlSourceName()->getName();
                int ctrlIdx = -1;
                for (int k = 0; k < voltageSources.size(); ++k) {
                    if (voltageSources[k]->getName() == ctrl) {
                        ctrlIdx = k;
                        break;
                    }
                }
                double gain = ee->getGain();
                if (ctrlIdx != -1) {
                    if (pi != -1) B[pi][ctrlIdx] += gain;
                    if (ni != -1) B[ni][ctrlIdx] -= gain;
                }
            }
            else if (type == "DiodeD" || type == "DiodeZ") {
                auto d = dynamic_pointer_cast<Diode>(e);
                if (d && d->isAssumedOn() && vsIndex < m) {
                    if (pi != -1) B[pi][vsIndex] = 1.0;
                    if (ni != -1) B[ni][vsIndex] = -1.0;
                    if (pi != -1) C[vsIndex][pi] = 1.0;
                    if (ni != -1) C[vsIndex][ni] = -1.0;
                    F[vsIndex] = (type == "DiodeZ") ? 0.7 : 0.0;
                    vsIndex++;
                }
            }
        }
        vector<vector<double>> A(matrixSize, vector<double>(matrixSize, 0.0));
        vector<double> Z(matrixSize, 0.0);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) A[i][j] = G[i][j];
            for (int j = 0; j < m; j++) A[i][n + j] = B[i][j];
            Z[i] = I[i];
        }
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) A[n + i][j] = C[i][j];
            for (int j = 0; j < m; j++) A[n + i][n + j] = D[i][j];
            Z[n + i] = F[i];
        }
        vector<double> solution = solveLinearSystem2(A, Z);
        for (shared_ptr<Node>& node : Wire::allNodes) {
            if (!node) continue;

            auto it = nodeIndex.find(node->getName());
            if (it != nodeIndex.end()) {
                int nodeIdx = it->second;

                if (node->getIsGround()) {
                    node->setVoltage(0.0);
                }
                else if (nodeIdx >= 0 && nodeIdx < solution.size()) {
                    node->setVoltage(solution[nodeIdx]);
                }
                else {
                    node->setVoltage (0.0);
                }
            }
        }

        size_t current_solution_idx = n;

        for (auto &vs: voltageSources) {
            if (vs && current_solution_idx < solution.size()) {
                vs->setCurrent(solution[current_solution_idx++]);
            } else {
                current_solution_idx++;
            }
        }

        for (auto &diode: onDiodes) {
            if (diode && current_solution_idx < solution.size()) {
                diode->setCurrent(solution[current_solution_idx++]);
            } else {
                current_solution_idx++;
            }
        }

        for (auto &e: elements) {
            if (e->getType() == "Resistor") {
                e->setCurrent(e->getVoltage() / e->getValue());
            }
        }
//------------------
        stringstream ss;
        ss << fixed << setprecision(6);
        for (auto &i:nodeIndex) {
            ss<<"V("<<i.first<<") = "<<solution[i.second]<<endl;
        }
        for (auto &i:elements) {
            ss<<"I("<<i->getName()<<") = "<<i->getCurrent()<<endl;
        }

        return ss.str();
    }

    string analyzeOp(messageBox &opBox, int componentId, vector<shared_ptr<Element>>&elements,vector<shared_ptr<LabelNet>>&labels) {
        int numDiodes = count_if(elements.begin(), elements.end(),
                                 [](auto e) { return e->getType() == "DiodeD" || e->getType() == "DiodeZ"; });

        findError(elements, Wire::allNodes);

        ofstream outFile("data/log.txt");

        if (!outFile.is_open()) {
            cerr << "Unable to open log.txt for writing" << endl;
            return "Error: Could not open log file";
        }

        int totalStates = 1 << numDiodes;
        for (int state = 0; state < totalStates; ++state) {
            int bit = 0;
            for (auto &e: elements) {
                if (e->getType() == "DiodeD" || e->getType() == "DiodeZ") {
                    auto d = dynamic_pointer_cast<Diode>(e);
                    if (d) {
                        bool on = (state >> bit) & 1;
                        d->assumeState(on);
                    }
                    bit++;
                }
            }
            string data= op(componentId, elements,labels);
            if (Truestate(elements)) {
                outFile<<data;
                opBox.setMessage(data);
                opBox.setTitle(".OP!");
                opBox.show();
                return "op ok";
            }
        }
        opBox.setMessage("No valid diode state found.");
        opBox.setTitle(".OP!");
        opBox.show();
        return "No valid diode state found.";
    }

    string DCSweep(int componentId, vector<shared_ptr<Element>>& elements, string SourceName, string tStart, string tEnd, string step,vector<shared_ptr<LabelNet>>&labels) {
        findError(elements, Wire::allNodes);

        ofstream outFile("data/log.txt");
        if (!outFile.is_open()) {
            cerr << "Unable to open log.txt for writing" << endl;
            return "Error: Could not open log file";
        }

        auto findElement = [&elements](string name) -> shared_ptr<Element> {
            for (int i = 0; i < elements.size(); ++i) {
                if (elements[i]->getName() == name) {
                    return elements[i];
                }
            }
            return nullptr;
        };

        double TStart = 0;
        double TStop = 0;
        double Step = 0;
        if (tStart != "0")
            TStart = unitHandler3(tStart, "start time");
        TStop = unitHandler3(tEnd, "stop time");
        Step = unitHandler3(step, "time steps");

        shared_ptr<Element> E = findElement(SourceName);
        int numDiodes = count_if(elements.begin(), elements.end(),
                                 [](auto e) { return e->getType() == "DiodeD" || e->getType() == "DiodeZ"; });

// ذخیره داده‌های خروجی به فرمت مشابه transient
        map<string, vector<pair<double, double>>> voltageResults;
        map<string, vector<pair<double, double>>> currentResults;

        if (numDiodes != 0) {
            for (double t = TStart; t <= TStop + 1e-12; t += Step) {
                E->setValue(t);
                if (t == 0) {
                    E->setValue(t + 1e-12);
                }

                int totalStates = 1 << numDiodes;
                for (int state = 0; state < totalStates; ++state) {
                    int bit = 0;
                    for (auto &e : elements) {
                        if (e->getType() == "DiodeD" || e->getType() == "DiodeZ") {
                            auto d = dynamic_pointer_cast<Diode>(e);
                            if (d) {
                                bool on = (state >> bit) & 1;
                                d->assumeState(on);
                            }
                            bit++;
                        }
                    }

                    string data = op(componentId, elements,labels);
                    if (Truestate(elements)) {
// ذخیره نتایج ولتاژ
                        for (shared_ptr<Node>& node : Wire::allNodes) {
                            if (!node->getIsGround()) {
                                string nodeName = "V(" + node->getName() + ")";
                                voltageResults[nodeName].push_back({t, node->getVoltage()});
                            }
                        }

// ذخیره نتایج جریان
                        for (auto &elem : elements) {
                            string elemName = "I(" + elem->getName() + ")";
                            currentResults[elemName].push_back({t, elem->getCurrent()});
                        }

                        break; // حالت صحیح پیدا شد
                    }
                }
            }
        }
        else {
            for (double t = TStart; t <= TStop + 1e-12; t += Step) {
                E->setValue(t);
                if (t == 0) {
                    E->setValue(t + 1e-12);
                }

                string data = op(componentId, elements,labels);

// ذخیره نتایج ولتاژ
                for (shared_ptr<Node>& node : Wire::allNodes) {
                    if (!node->getIsGround()) {
                        string nodeName = "V(" + node->getName() + ")";
                        voltageResults[nodeName].push_back({t, node->getVoltage()});
                    }
                }

// ذخیره نتایج جریان
                for (auto &elem : elements) {
                    string elemName = "I(" + elem->getName() + ")";
                    currentResults[elemName].push_back({t, elem->getCurrent()});
                }
            }
        }

// نوشتن نتایج در فایل به فرمت مشابه transient
        for (const auto& entry : voltageResults) {
            outFile << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << point.first << "," << point.second << endl;
            }
        }

        for (const auto& entry : currentResults) {
            outFile << entry.first << endl;
            for (const auto& point : entry.second) {
                outFile << point.first << "," << point.second << endl;
            }
        }

        outFile.close();
        return "DCSweep ok";
    }
}

using namespace Analyze;

//////---------------------------------------
namespace Network{
    class TCPServer {
    public:
        TCPServer(int port) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2,2), &wsaData);

            ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_addr.s_addr = INADDR_ANY;

            bind(ServerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
            listen(ServerSocket, SOMAXCONN);
        }

        void acceptClient() {
            sockaddr_in clientAddr{};
            int clientSize = sizeof(clientAddr);
            clientSocket = accept(ServerSocket, (sockaddr*)&clientAddr, &clientSize);
            std::cout << "Client connected!\n";
        }

        void sendMessage(const std::string& msg) {
            send(clientSocket, msg.c_str(), msg.size(), 0); // اصلاح شده: ارسال به clientSocket
        }

        void receiveMessage() {
            char buffer[1024] = {};
            recv(clientSocket, buffer, sizeof(buffer), 0); // اصلاح شده: دریافت از clientSocket
            std::cout << "Received from client: " << buffer << "\n";
        }

        ~TCPServer() {
            closesocket(clientSocket);
            closesocket(ServerSocket);
            WSACleanup();
        }

    public:
        SOCKET ServerSocket;
        SOCKET clientSocket;
    };

    class TCPClient {
    public:
        TCPClient(const std::string& ip, int port) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);

            clientSocket = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());

            connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        }

        void sendMessage(const std::string& msg) {
            send(clientSocket, msg.c_str(), msg.size(), 0);
        }

        string receiveMessage() {
            char buffer[1024] = {};
            recv(clientSocket, buffer, sizeof(buffer), 0);
            std::cout << "Received from server: " << buffer << "\n";
            return buffer;
        }

        ~TCPClient() {
            closesocket(clientSocket);
            WSACleanup();
        }

    public:
        SOCKET clientSocket;
    };

    //----------------------------------

    void startServer() {
        TCPServer server(PORT);
        std::cout << "Server waiting for client...\n";
        server.acceptClient();

        // حالا می‌توانیم هم ارسال کنیم هم دریافت
        server.sendMessage("Hello from Server!");
        //server.receiveMessage();
    }

    void startClient() {
        TCPClient client(IP, PORT);
        client.receiveMessage();
        //client.sendMessage("Hello from Client!");
    }

    //----------------------------------
    void startServerSendVoltageSource(shared_ptr<SinVoltageSource> e) {
        TCPServer server(PORT);
        ostringstream data;
        data<<e->getOffset()<<","<<e->getAmplitude()<<","<<e->getFrequency();
        std::cout << "Server waiting for client...\n";
        server.acceptClient();
        server.sendMessage(data.str());
    }

    void startClientReceiveVoltageSource(shared_ptr<SinVoltageSource> e) {
        TCPClient client(IP, PORT);
        string data;
        auto parseDoubles=[](const std::string& str)->vector<double> {
            vector<double> values;
            istringstream iss(str);
            string token;
            while (std::getline(iss, token, ',')) {
                values.push_back(std::stod(token));
            }
            return values;
        };
        data=client.receiveMessage();
        vector<double> x=parseDoubles(data);
        e->setOffset(x[0]);
        e->setAmplitude(x[1]);
        e->setFrequency(x[2]);
    }

    //----------------------------------

    std::mutex circuit_data_mutex;

    void startServerSendCircuit(Wire &wire, vector<shared_ptr<LabelNet>>&labels, vector<shared_ptr<Element>>&elements, vector<shared_ptr<GNDSymbol>>&gndSymbols) {
        TCPServer server(PORT);
        std::cout << "Server waiting for client...\n";
        server.acceptClient();

        // قفل کردن موتکس قبل از دسترسی به داده‌های مشترک برای سریالایز
        std::lock_guard<std::mutex> lock(circuit_data_mutex);

        // سریالایز کردن داده
        std::stringstream ss(std::ios::binary | std::ios::out | std::ios::in);
        {
            cereal::BinaryOutputArchive archive(ss);
            archive(wire);
            archive(labels);
            archive(elements);
            archive(gndSymbols);

            // متغیرهای استاتیک
            archive(liine::count);
            archive(Wire::allNodes);
            archive(Wire::Lines);
        }
// قفل در انتهای این بلوک (scope) به صورت خودکار آزاد می‌شود

        std::string serializedData = ss.str();

// ارسال طول داده اول
        uint32_t dataSize = htonl(serializedData.size());
        send(server.clientSocket, reinterpret_cast<const char*>(&dataSize), sizeof(dataSize), 0);

// ارسال خود داده
        send(server.clientSocket, serializedData.c_str(), serializedData.size(), 0);
        std::cout << "Server: Circuit data sent.\n";
    }

    void startClientReceiveCircuit(Wire &wire, vector<shared_ptr<LabelNet>>&labels, vector<shared_ptr<Element>>&elements, vector<shared_ptr<GNDSymbol>>&gndSymbols) {
        TCPClient client(IP, PORT);
        std::cout << "Client: Connecting to server...\n";

// دریافت طول داده
        uint32_t dataSize;
        if (recv(client.clientSocket, reinterpret_cast<char*>(&dataSize), sizeof(dataSize), 0) <= 0) {
            std::cerr << "Client: Error receiving data size or connection closed.\n";
            return;
        }
        dataSize = ntohl(dataSize);
        std::cout << "Client: Received data size: " << dataSize << " bytes.\n";

// دریافت داده
        std::vector<char> buffer(dataSize);
        size_t totalReceived = 0;
        while (totalReceived < dataSize) {
            int bytesReceived = recv(client.clientSocket, buffer.data() + totalReceived, dataSize - totalReceived, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Client: Error receiving data or connection closed mid-transfer.\n";
                return;
            }
            totalReceived += bytesReceived;
        }
        std::cout << "Client: All data received. Total: " << totalReceived << " bytes.\n";

// قفل کردن موتکس قبل از دسترسی به داده‌های مشترک برای دیسریالایز
        std::lock_guard<std::mutex> lock(circuit_data_mutex);

// دیسریال کردن
        std::stringstream ss(std::string(buffer.begin(), buffer.end()),
                             std::ios::binary | std::ios::out | std::ios::in);

        try {
            cereal::BinaryInputArchive archive(ss);
            archive(
                    wire,
                    labels,
                    elements,
                    gndSymbols,
                    liine::count,
                    Wire::allNodes,
                    Wire::Lines
            );
            std::cout << "Client: Circuit data deserialized successfully.\n";
        }
        catch (const cereal::Exception& e) {
            std::cerr << "Client: Deserialization error: " << e.what() << '\n';
        }
        catch (const std::exception& e) {
            std::cerr << "Client: Standard error during deserialization: " << e.what() << '\n';
        }
    }
//----------------------------------

    void startServerSendAnalyze(const vector<string>& analyzeData) {
        TCPServer server(PORT);
        std::cout << "Server waiting for client...\n";
        server.acceptClient();

        // سریالایز کردن داده
        std::stringstream ss(std::ios::binary | std::ios::out | std::ios::in);
        {
            cereal::BinaryOutputArchive archive(ss);
            archive(analyzeData); // سریال کردن کل وکتور
        }

        std::string serializedData = ss.str();

        // ارسال طول داده اول
        uint32_t dataSize = htonl(serializedData.size());
        send(server.clientSocket, reinterpret_cast<const char*>(&dataSize), sizeof(dataSize), 0);

        // ارسال خود داده
        send(server.clientSocket, serializedData.c_str(), serializedData.size(), 0);
    }


    vector<string> startClientReceiveAnalyze(){
        TCPClient client(IP, PORT);

        // دریافت طول داده
        uint32_t dataSize;
        recv(client.clientSocket, reinterpret_cast<char*>(&dataSize), sizeof(dataSize), 0);
        dataSize = ntohl(dataSize);

        // دریافت داده
        std::vector<char> buffer(dataSize);
        recv(client.clientSocket, buffer.data(), dataSize, 0);

        // دیسریال کردن
        std::stringstream ss(std::string(buffer.begin(), buffer.end()),
                             std::ios::binary | std::ios::out | std::ios::in);

        vector<string> analyzeData;
        {
            cereal::BinaryInputArchive archive(ss);
            archive(analyzeData); // دیسریال کردن کل وکتور
        }

        return analyzeData;
    }

}

using namespace Network;
//////---------------------------------------
void addElement(messageBox &error,vector<shared_ptr<Element>>&e,int x,int y ,string type,string name,string value,string model="",string gain="",string sourceElement="",string sourceNodeP="",string sourceNodeN="",string nameS="",string posNode="",string negNode="",string phase="",string Basefr=""){
    auto unitHandler=[](string input, string type)->double {
        regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
        smatch match;
        if (regex_match(input, match, pattern)) {
            double value = stod(match[1]);
            string unit = match[2].str();
            double multiplier = 1.0;
            if (unit == "n") multiplier = 1e-9;
            else if (unit == "u") multiplier = 1e-6;
            else if (unit == "m") multiplier = 1e-3;
            else if (unit == "k") multiplier = 1e3;
            else if (unit == "M" || unit == "Meg") multiplier = 1e6;
            else if (unit == "G") multiplier = 1e9;
            if (value <= 0) {
                throw invalidValue(type);
            }
            return value * multiplier;
        } else {
            throw invalidValue(type);
        }
        return -1.0;
    };
    auto findElement=[&e] (string name)->shared_ptr<Element>{
        for (int i = 0; i < e.size(); ++i) {
            if(e[i]->getName()==name) {
                return e[i];
            }
        }
        return nullptr;
    };

    snapToGrid(x,y);

    shared_ptr<Node> posNodeE=Wire::findNode(x+stepX*2,y);

    shared_ptr<Node> negNodeE=Wire::findNode(x-stepX*2,y);

    if(type=="Resistor"){
        double Value = unitHandler(value, "Resistance");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<Resistor> r = dynamic_pointer_cast<Resistor>(E);
        if (r) {
            throw duplicateElementName("Resistor", name);
        }
        shared_ptr<Resistor>R = make_shared<Resistor>(x,y,name,Value,negNodeE,posNodeE);
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="Capacitor"){
        double Value = unitHandler(value, "Capacitance");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<Capacitor> r = dynamic_pointer_cast<Capacitor>(E);
        if (r) {
            throw duplicateElementName("Capacitor", name);
        }
        shared_ptr<Capacitor>R = make_shared<Capacitor>(x,y,name,Value,negNodeE,posNodeE);
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="Inductor"){
        double Value = unitHandler(value, "Inductance");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<Inductor> r = dynamic_pointer_cast<Inductor>(E);
        if (r) {
            throw duplicateElementName("Inductor", name);
        }
        shared_ptr<Inductor>R = make_shared<Inductor>(x,y,name,Value,negNodeE,posNodeE);
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="VoltageSource"){
        double Value = unitHandler(value, "Voltage");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<VoltageSource> r = dynamic_pointer_cast<VoltageSource>(E);
        if (r) {
            throw duplicateElementName("VoltageSource", name);
        }
        shared_ptr<VoltageSource>R = make_shared<VoltageSource>(x,y,name,Value,negNodeE,posNodeE);
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="AcVoltageSource"){
        double Value = unitHandler(value, "Voltage");
        double Phase= unitHandler3(phase, "Voltage");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<VoltageSource> r = dynamic_pointer_cast<VoltageSource>(E);
        if (r) {
            throw duplicateElementName("VoltageSource", name);
        }
        shared_ptr<VoltageSource>R = make_shared<VoltageSource>(x,y,name,Value,negNodeE,posNodeE);
        R->isAcSource=true;
        R->phase=Phase;
        R->AcValue={(Value*cos(R->phase),Value*sin(R->phase))};
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="PhaseVoltageSource"){
        double Value = unitHandler(value, "Voltage");
        double BaseFry= unitHandler(Basefr, "Voltage");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<VoltageSource> r = dynamic_pointer_cast<VoltageSource>(E);
        if (r) {
            throw duplicateElementName("VoltageSource", name);
        }
        shared_ptr<VoltageSource>R = make_shared<VoltageSource>(x,y,name,Value,negNodeE,posNodeE);
        R->isPhaseSource=true;
        R->fr=BaseFry;
//        R->AcValue={(Value*cos(R->phase),Value*sin(R->phase))};
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="PWLVoltageSource"){

        auto E = findElement(name);

        auto vs = dynamic_pointer_cast<VoltageSource>(E);

        if (vs) {

            throw duplicateElementName("VoltageSource", name);

        }

        auto Vs = make_shared<PWLVoltageSource>(x,y,name,"data/PWL.txt",negNodeE,posNodeE);

        Vs->value="PWL";

        e.push_back(Vs);

    }

    else if(type=="CurrentSource"){
        double Value = unitHandler(value, "Voltage");
        shared_ptr<Element> E = findElement(name);
        shared_ptr<CurrentSource> r =dynamic_pointer_cast<CurrentSource>(E);
        if (r) {
            throw duplicateElementName("CurrentSource", name);
        }
        shared_ptr<CurrentSource>R = make_shared<CurrentSource>(x,y,name,Value,negNodeE,posNodeE);
        R->value=value;
//تنظیم نود هاش
        e.push_back(R);
    }

    else if(type=="Diode"){
        shared_ptr<Element>E = findElement(name);
        shared_ptr<Diode>r = dynamic_pointer_cast<Diode>(E);
        if (r) {
            throw duplicateElementName("Diode", name);
        }
        shared_ptr<Diode> R;
        if(model!="Z"&&model!="D"){
            throw ErrorInput();
        }
        if (model=="Z"){
            R = make_shared<Diode>(x,y,name,model,0.7,posNodeE,negNodeE);
        }
        else if (model=="D"){
            R = make_shared<Diode>(x,y,name,model,0,posNodeE,negNodeE);
        }
        else {
            throw diodeModelNotFound(model);
        }
        R->value=model;
//تنظیم نودها
        e.push_back(R);
    }

    else if(type=="CCCS"){
        double g= stod(gain);
        shared_ptr<Element>E = findElement(name);
        if (E) {
            throw duplicateElementName("Element", name);
        }
        shared_ptr<Element> f = findElement(sourceElement);
        if (!f){
            throw elementNotFound2(nameS);
        }
        shared_ptr<CCCS> R=make_shared<CCCS>(x,y,name,g,f,negNodeE,posNodeE);
        R->value="CCCS(g: "+gain+",e: "+nameS+")";
        e.push_back(R);
    }

    else if(type=="CCVS"){
        double g= stod(gain);
        shared_ptr<Element>E = findElement(name);
        if (E) {
            throw duplicateElementName("Element", name);
        }
        shared_ptr<Element> f = findElement(sourceElement);
        if (!f){
            throw elementNotFound2(nameS);
        }
        shared_ptr<CCVS> R= make_shared<CCVS>(x,y,name,g,f,negNodeE,posNodeE);
        R->value="CCVS(g: "+gain+",e: "+nameS+")";;
        e.push_back(R);
    }

    else if(type=="VCCS"){
        double g= stod(gain);
        shared_ptr<Node> P=Wire::findNodeWhitname(sourceNodeP)[0];
        shared_ptr<Node> N=Wire::findNodeWhitname(sourceNodeN)[0];
        if(!N){
            throw nodeNotFound(negNode);
        }
        if(!P){
            throw nodeNotFound(posNode);
        }
        shared_ptr<Element >E = findElement(name);
        if (E) {
            throw duplicateElementName("Element", name);
        }
        shared_ptr<VCCS> R=make_shared<VCCS>(x,y,name,g,P,N,negNodeE,posNodeE);
        R->value="VCCS(g: "+gain+",posN: "+posNode+",negN:"+negNode+")";;
        e.push_back(R);
    }

    else if(type=="VCVS"){
        double g= stod(gain);
        shared_ptr<Node> P=Wire::findNodeWhitname(sourceNodeP)[0];
        shared_ptr<Node> N=Wire::findNodeWhitname(sourceNodeN)[0];
        if(!N){
            throw nodeNotFound(negNode);
        }
        if(!P){
            throw nodeNotFound(posNode);
        }

        shared_ptr<Element>E = findElement(name);
        if (E) {
            throw duplicateElementName("Element", name);
        }
        shared_ptr<VCVS>R= make_shared<VCVS>(x,y,name,g,P,N,negNodeE,posNodeE);
        R->value="VCVS(g: "+gain+",posN: "+posNode+",negN:"+negNode+")";
//تنظیم نود ها
        e.push_back(R);
    }
}

void addElementTime(messageBox &error,vector<shared_ptr<Element>>&e,int x,int y ,string type,string name,
                    string frequency,string vOffset,string vAmplitude,
                    string Vinitial, string Von,string Tdelay, string Trise,string Tfall, string Ton,string Tperiod, string Ncycles = "0",
                    string tPulse="",string area=""){
    auto unitHandler=[](string input, string type)->double {
        regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
        smatch match;
        if (regex_match(input, match, pattern)) {
            double value = stod(match[1]);
            string unit = match[2].str();
            double multiplier = 1.0;
            if (unit == "n") multiplier = 1e-9;
            else if (unit == "u") multiplier = 1e-6;
            else if (unit == "m") multiplier = 1e-3;
            else if (unit == "k") multiplier = 1e3;
            else if (unit == "M" || unit == "Meg") multiplier = 1e6;
            else if (unit == "G") multiplier = 1e9;
            if (value <= 0) {
                throw invalidValue(type);
            }
            return value * multiplier;
        } else {
            throw invalidValue(type);
        }
        return -1.0;
    };
    auto findElement=[&e] (string name)->shared_ptr<Element>{
        for (int i = 0; i < e.size(); ++i) {
            if(e[i]->getName()==name) {
                return e[i];
            }
        }
        return nullptr;
    };
    auto unitHandler2=[](string input, string type="time") {
        regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
        smatch match;
        if (regex_match(input, match, pattern)) {
            double value = stod(match[1]);
            string unit = match[2].str();
            double multiplier = 1.0;
            if (unit == "n") multiplier = 1e-9;
            else if (unit == "u") multiplier = 1e-6;
            else if (unit == "m") multiplier = 1e-3;
            else if (unit == "k") multiplier = 1e3;
            else if (unit == "M" || unit == "Meg") multiplier = 1e6;
            else if (unit == "G") multiplier = 1e9;
            if (value < 0) {
                throw invalidValue(type);
            }
            return value * multiplier;
        } else {
            throw invalidValue(type);
        }
        return -1.0;
    };

    snapToGrid(x,y);
    auto posNodeE = Wire::findNode(x+stepX*2,y);
    auto negNodeE = Wire::findNode(x-stepX*2,y);

    if(type=="DeltaVoltageSource"){
        double Value = unitHandler(area, "Voltage");
        auto E = findElement(name);
        auto vs = dynamic_pointer_cast<VoltageSource>(E);
        if (vs) {
            throw duplicateElementName("VoltageSource", name);
        }
        auto Vs = make_shared<deltaVoltageSource>(x,y,name,unitHandler2(tPulse),0.001,Value,negNodeE,posNodeE);
        Vs->value="Delta(area: "+area+",tPulse: "+tPulse+")";
        e.push_back(Vs);
    }

    else if(type=="DeltaCurrentSource"){
        double Value = unitHandler(area, "Current");
        auto E = findElement(name);
        auto cs = dynamic_pointer_cast<CurrentSource>(E);
        if (cs) {
            throw duplicateElementName("CurrentSource", name);
        }
        auto Is = make_shared<deltaCurrentSource>(x,y,name,unitHandler2(tPulse),0.001,Value,negNodeE,posNodeE);
        Is->value="Delta(area: "+area+",tPulse: "+tPulse+")";
        e.push_back(Is);
    }

    else if(type=="SineVoltageSource"){
        double freq = unitHandler(frequency, "frequency");
        auto E = findElement(name);
        auto vs = dynamic_pointer_cast<VoltageSource>(E);
        if (vs) {
            throw duplicateElementName("VoltageSource", name);
        }
        auto Vs = make_shared<SinVoltageSource>(x,y,name,stod(vOffset),stod(vAmplitude),freq,negNodeE,posNodeE);
        Vs->value="Sine(f: "+frequency+",Amp: "+vAmplitude+",off: "+vOffset+")";
        e.push_back(Vs);
    }

    else if(type=="SineCurrentSource"){
        double freq = unitHandler(frequency, "frequency");
        auto E = findElement(name);
        auto cs = dynamic_pointer_cast<CurrentSource>(E);
        if (cs) {
            throw duplicateElementName("CurrentSource", name);
        }
        auto Is = make_shared<SinCurrentSource>(x,y,name,stod(vOffset),stod(vAmplitude),freq,negNodeE,posNodeE);
        Is->value="Sine(f: "+frequency+",Amp: "+vAmplitude+",off: "+vOffset+")";
        e.push_back(Is);
    }

    else if(type=="PulseVoltageSource"){
        auto E = findElement(name);
        auto vs = dynamic_pointer_cast<VoltageSource>(E);
        if (vs) {
            throw duplicateElementName("VoltageSource", name);
        }
        auto Vs = make_shared<PulseVoltageSource>(x,y,name,
                                                  stod(Vinitial), stod(Von),
                                                  unitHandler2(Tdelay, "time"), unitHandler2(Trise, "time"),
                                                  unitHandler2(Tfall, "time"), unitHandler2(Ton, "time"),
                                                  unitHandler2(Tperiod, "time"), unitHandler2(Ncycles),
                                                  negNodeE,posNodeE);
        Vs->value="Pulse(Vini: "+Vinitial+",Von: "+Von+",Td: "+Tdelay+
                  ",Tr: "+Trise+",Tf: "+Tfall+",Ton: "+Ton+",Tp: "+Tperiod+")";
        e.push_back(Vs);
    }

    else if(type=="PulseCurrentSource"){
        auto E = findElement(name);
        auto cs = dynamic_pointer_cast<CurrentSource>(E);
        if (cs) {
            throw duplicateElementName("CurrentSource", name);
        }
        auto Is = make_shared<PulseCurrentSource>(x,y,name,
                                                  stod(Vinitial), stod(Von),
                                                  unitHandler2(Tdelay, "time"), unitHandler2(Trise, "time"),
                                                  unitHandler2(Tfall, "time"), unitHandler2(Ton, "time"),
                                                  unitHandler2(Tperiod, "time"), unitHandler2(Ncycles),
                                                  negNodeE,posNodeE);
        Is->value="Pulse(Vini: "+Vinitial+",Von: "+Von+",Td: "+Tdelay+
                  ",Tr: "+Trise+",Tf: "+Tfall+",Ton: "+Ton+",Tp: "+Tperiod+")";
        e.push_back(Is);
    }

}

vector<Button> createToolbar() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    vector<Button> toolbar;
    int btnWidth = 60;
    int btnHeight = 30;
    int margin = 5;
    int startX = 5;
    int y = 5;
    Button btnNew(startX, y, btnWidth, btnHeight, font, "File");
    btnNew.onClick = []() { SDL_Log("New File clicked"); };
    toolbar.push_back(btnNew);
//    Button btnOpen(startX + (btnWidth + margin) * 1, y, btnWidth, btnHeight, font, "Open");
//    btnOpen.onClick = []() { SDL_Log("Open clicked"); };
//    toolbar.push_back(btnOpen);
    Button btnSave(startX + (btnWidth + margin) * 1, y, btnWidth, btnHeight, font, "Save");
    btnSave.onClick = []() { SDL_Log("Save clicked"); };
    toolbar.push_back(btnSave);
    Button btnlib(startX + (btnWidth + margin) * 2, y, btnWidth, btnHeight, font, "lib Folder");
    btnlib.onClick = []() { SDL_Log("Save clicked"); };
    toolbar.push_back(btnlib);
    Button btnNet(startX + (btnWidth + margin) * 3, y, btnWidth, btnHeight, font, "NetWork");
    btnNet.onClick = []() { SDL_Log("network clicked"); };
    toolbar.push_back(btnNet);
    Button btnExit(startX + (btnWidth + margin) * 4, y, btnWidth, btnHeight, font, "Exit");
    btnExit.onClick = [&]() { SDL_Event quitEvent; quitEvent.type = SDL_QUIT; SDL_PushEvent(&quitEvent); };
    toolbar.push_back(btnExit);
    return toolbar;
}

vector<Button> createLibrary() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    vector<Button> library;
    int btnWidth = 90;
    int btnHeight = 20;
    int margin = 5;
    int startY = 60;
    int x = 1366 - btnWidth - 5;
    Button btnNew(x, startY + (btnHeight + margin) * 0, btnWidth, btnHeight, font, "Component");
    library.push_back(btnNew);
    Button btnWire(x, startY + (btnHeight + margin) * 1, btnWidth, btnHeight, font, "Wire");
    library.push_back(btnWire);
    Button btnDelete(x, startY + (btnHeight + margin) * 2, btnWidth, btnHeight, font, "Delete");
    library.push_back(btnDelete);
    Button btnAnalyze(x, startY + (btnHeight + margin) * 3, btnWidth, btnHeight, font, "Analyze");
    library.push_back(btnAnalyze);
    Button btnGnd(x, startY + (btnHeight + margin) * 4, btnWidth, btnHeight, font, "GND");
    library.push_back(btnGnd);
    Button btnLabelNet(x, startY + (btnHeight + margin) * 5, btnWidth, btnHeight, font, "Label Net");
    library.push_back(btnLabelNet);
    Button VProbe(x, startY + (btnHeight + margin) * 6, btnWidth, btnHeight, font, "V Probe");
    library.push_back(VProbe);
    Button CProbe(x, startY + (btnHeight + margin) * 7, btnWidth, btnHeight, font, "C Probe");
    library.push_back(CProbe);
    Button PProbe(x, startY + (btnHeight + margin) * 8, btnWidth, btnHeight, font, "P Probe");
    library.push_back(PProbe);
    Button Plot(x, startY + (btnHeight + margin) * 9, btnWidth, btnHeight, font, "Plot");
    library.push_back(Plot);
    Button mirror(x, startY + (btnHeight + margin) * 10, btnWidth, btnHeight, font, "Mirror");
    library.push_back(mirror);

    return library;
}

PopupMenu createSaveMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu saveMenu(font, 180);
    saveMenu.addItem("Save", [](){ SDL_Log("Do Save"); });
    saveMenu.addItem("Save As...", [](){ SDL_Log("Do Save As"); });
    saveMenu.addItem("Export", [](){ SDL_Log("Do Export"); });
    return saveMenu;
}

PopupMenu createNetWorkMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu saveMenu(font, 180);
    saveMenu.addItem("Server", [](){ SDL_Log("Do Server"); });
    saveMenu.addItem("Client", [](){ SDL_Log("Do Client"); });
    return saveMenu;
}

PopupMenu createServerMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu saveMenu(font, 180);
    saveMenu.addItem("send VoltageSource", [](){ SDL_Log("send VoltageSource"); });
    saveMenu.addItem("send Circuit", [](){ SDL_Log("send Circuit"); });
    saveMenu.addItem("send analyze", [](){ SDL_Log("send analyze"); });
    return saveMenu;
}

PopupMenu createClientMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu saveMenu(font, 180);
    saveMenu.addItem("receive VoltageSource", [](){ SDL_Log("receive VoltageSource");});
    saveMenu.addItem("receive Circuit", [](){ SDL_Log("receive Circuit"); });
    saveMenu.addItem("receive analyze", [](){ SDL_Log("receive analyze"); });
    return saveMenu;
}

PopupMenu createFileMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu saveMenu(font, 180);
    saveMenu.addItem("open", [](){ SDL_Log("Do Save"); });
    saveMenu.addItem("New", [](){ SDL_Log("Do Save As"); });
    //saveMenu.addItem("Export", [](){ SDL_Log("Do Export"); });
    return saveMenu;
}

PopupMenu createComponentMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu componentMenu(font, 130);
    componentMenu.addItem("R", [](){ SDL_Log("R"); });
    componentMenu.addItem("C", [](){ SDL_Log("C"); });
    componentMenu.addItem("L", [](){ SDL_Log("L"); });
    componentMenu.addItem("D", [](){ SDL_Log("D"); });
    componentMenu.addItem("V", [](){ SDL_Log("V"); });
    componentMenu.addItem("Cu", [](){ SDL_Log("Cu"); });
    componentMenu.addItem("CCCS", [](){ SDL_Log("CCCS"); });
    componentMenu.addItem("CCVS", [](){ SDL_Log("CCVS"); });
    componentMenu.addItem("VCCS", [](){ SDL_Log("VCCS"); });
    componentMenu.addItem("VCVS", [](){ SDL_Log("VCVS"); });
    componentMenu.addItem("DeltaVoltageSource", [](){ SDL_Log("DeltaVoltageSource"); });
    componentMenu.addItem("DeltaCurrentSource", [](){ SDL_Log("DeltaCurrentSource"); });
    componentMenu.addItem("SineVoltageSource", [](){ SDL_Log("SineVoltageSource"); });
    componentMenu.addItem("SineCurrentSource", [](){ SDL_Log("SineCurrentSource"); });
    componentMenu.addItem("PulseVoltageSource", [](){ SDL_Log("PulseVoltageSource"); });
    componentMenu.addItem("PulseCurrentSource", [](){ SDL_Log("PulseCurrentSource"); });
    componentMenu.addItem("AcVoltageSource", [](){ SDL_Log("AcVoltageSource"); });
    componentMenu.addItem("PhaseVoltageSource", [](){ SDL_Log("PhaseVoltageSource"); });
    componentMenu.addItem("PWLVoltageSource", [](){ SDL_Log("PWLVoltageSource"); });

    return componentMenu;
}

PopupMenu createAnalyzeMenu() {
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    PopupMenu componentMenu(font, 90);
    componentMenu.addItem("OP", [](){ SDL_Log("I"); });
    componentMenu.addItem("Transient", [](){ SDL_Log("R"); });
    componentMenu.addItem("DC Sweep", [](){ SDL_Log("C"); });
    componentMenu.addItem("Ac Sweep", [](){ SDL_Log("V"); });
    componentMenu.addItem("Phase Sweep", [](){ SDL_Log("D"); });
    return componentMenu;
}

// تابع به‌روزرسانی رشته پروب‌ها
vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// تابع برای تشخیص شروع یک بخش جدید
int get_precedence(const std::string& op) {
    if (op == "+" || op == "-") return 1;
    if (op == "*" || op == "/") return 2;
    return 0;
}

bool is_operator(const std::string& token) {
    return token == "+" || token == "-" || token == "*" || token == "/";
}

// تابع tokenize_expression بهبود یافته برای پشتیبانی از (amp) و (phase)
std::vector<std::string> tokenize_expression(const std::string& expr) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < expr.length(); ++i) {
        if (isspace(expr[i])) continue;

        if ((expr[i] == 'V' || expr[i] == 'I') && i + 1 < expr.length() && expr[i+1] == '(') {
            size_t start = i;
            size_t end = expr.find(')', start);
            if (end != std::string::npos) {
                // بررسی وجود پسوندهای (amp) یا (phase)
                if (expr.substr(end + 1, 5) == "(amp)") {
                    end += 5;
                } else if (expr.substr(end + 1, 7) == "(phase)") {
                    end += 7;
                }
                tokens.push_back(expr.substr(start, end - start + 1));
                i = end;
            }
        } else if (isdigit(expr[i]) || expr[i] == '.') {
            size_t start = i;
            while (i + 1 < expr.length() && (isdigit(expr[i+1]) || expr[i+1] == '.')) {
                i++;
            }
            tokens.push_back(expr.substr(start, i - start + 1));
        } else if (std::string("+-*/()").find(expr[i]) != std::string::npos) {
            tokens.push_back(std::string(1, expr[i]));
        }
    }
    return tokens;
}

std::vector<std::string> infix_to_postfix(const std::vector<std::string>& tokens) {
    std::vector<std::string> output_queue;
    std::stack<std::string> operator_stack;

    for (const std::string& token : tokens) {
        // هر توکنی که با V یا I شروع شود یا عدد باشد، یک مقدار (value) است
        if (isdigit(token[0]) || token[0] == 'V' || token[0] == 'I') {
            output_queue.push_back(token);
        } else if (is_operator(token)) {
            while (!operator_stack.empty() && operator_stack.top() != "(" &&
                   get_precedence(operator_stack.top()) >= get_precedence(token)) {
                output_queue.push_back(operator_stack.top());
                operator_stack.pop();
            }
            operator_stack.push(token);
        } else if (token == "(") {
            operator_stack.push(token);
        } else if (token == ")") {
            while (!operator_stack.empty() && operator_stack.top() != "(") {
                output_queue.push_back(operator_stack.top());
                operator_stack.pop();
            }
            if (!operator_stack.empty()) operator_stack.pop();
        }
    }

    while (!operator_stack.empty()) {
        output_queue.push_back(operator_stack.top());
        operator_stack.pop();
    }
    return output_queue;
}

double evaluate_postfix(const std::vector<std::string>& postfix_tokens, const std::map<std::string, double>& signal_values) {
    std::stack<double> eval_stack;
    for (const std::string& token : postfix_tokens) {
        if (isdigit(token[0])) {
            eval_stack.push(stod(token));
        } else if (token[0] == 'V' || token[0] == 'I') {
            eval_stack.push(signal_values.at(token));
        } else if (is_operator(token)) {
            if (eval_stack.size() < 2) throw std::runtime_error("Invalid expression");
            double val2 = eval_stack.top(); eval_stack.pop();
            double val1 = eval_stack.top(); eval_stack.pop();
            if (token == "+") eval_stack.push(val1 + val2);
            else if (token == "-") eval_stack.push(val1 - val2);
            else if (token == "*") eval_stack.push(val1 * val2);
            else if (token == "/") eval_stack.push(val1 / val2);
        }
    }
    if (eval_stack.size() != 1) throw std::runtime_error("Invalid expression");
    return eval_stack.top();
}

// تابع جدید برای نگاشت نام سیگنال ورودی به کلید استفاده شده در data_map
std::string map_signal_key(const std::string& input_key) {
    size_t amp_pos = input_key.find("(amp)");
    if (amp_pos != std::string::npos) {
        return input_key.substr(0, amp_pos) + "(amplitude)";
    }
    // برای (phase) نام ورودی و کلید نقشه یکسان است، اما برای خوانایی اضافه می‌شود
    size_t phase_pos = input_key.find("(phase)");
    if (phase_pos != std::string::npos) {
        return input_key.substr(0, phase_pos) + "(phase)";
    }
    return input_key;
}

bool is_new_section(const std::string &line) {
    if (line.empty()) return false;
    return (line[0] == 'V' || line[0] == 'I') && line.size() > 1 && line[1] == '(';
}

void extract_data(const string &input_string, const vector<shared_ptr<Element>> &elements) {
    std::ifstream log_file("data/log.txt");
    std::ofstream data_file("data.txt");

    if (!log_file.is_open()) {
        std::cerr << "Error: Could not open log.txt" << std::endl;
        return;
    }
    if (!data_file.is_open()) {
        std::cerr << "Error: Could not create data.txt" << std::endl;
        return;
    }

// خواندن خطوط فایل log.txt
    std::vector<std::string> log_lines;
    std::string line;
    while (getline(log_file, line)) {
        if (!line.empty()) log_lines.push_back(line);
    }
    log_file.close();

// پردازش خطوط و ذخیره در map
    std::map<std::string, std::vector<std::string>> data_map;
    std::string current_key;
    std::vector<std::string> current_data;

    for (const std::string &log_line : log_lines) {
        if (is_new_section(log_line)) {
            if (!current_key.empty()) {
                data_map[current_key] = current_data;
            }
            current_key = log_line;
            current_data.clear();
        } else {
            current_data.push_back(log_line);
        }
    }
    if (!current_key.empty()) {
        data_map[current_key] = current_data;
    }

// تقسیم رشته ورودی به کلیدها
    std::vector<std::string> input_keys;
    std::istringstream iss(input_string);
    std::string key_line;
    while(getline(iss, key_line)) {
        if(!key_line.empty()) input_keys.push_back(key_line);
    }

// پردازش کلیدها
    for (const string &key : input_keys) {
        if (key.size() > 2 && key[0] == 'P' && key[1] == '(') {
// پردازش المان‌های توان (همانند قبل)
            string elementName = key.substr(2, key.size() - 3);
            shared_ptr<Element> itElem = nullptr;
            for (auto &el : elements) {
                if (el->name == elementName) {
                    itElem = el;
                    break;
                }
            }

            if (!itElem) {
                cerr << "Warning: Element '" << elementName << "' not found in element list" << endl;
                continue;
            }

            string n1 = "V(" + itElem->getNodeP()->getName() + ")";
            string n2 = "V(" + itElem->getNodeN()->getName() + ")";
            string Ikey = "I(" + itElem->name + ")";

            bool n1_is_zero = (n1 == "V(N00)");
            bool n2_is_zero = (n2 == "V(N00)");

            if ((!n1_is_zero && data_map.find(n1) == data_map.end()) ||
                (!n2_is_zero && data_map.find(n2) == data_map.end()) ||
                data_map.find(Ikey) == data_map.end()) {
                cerr << "Warning: Missing data for element '" << elementName << "'" << endl;
                continue;
            }

            auto &i_data = data_map[Ikey];

            data_file << key << '\n';
            for (size_t i = 0; i < i_data.size(); i++) {
                double t3, i3;
                sscanf(i_data[i].c_str(), "%lf,%lf", &t3, &i3);

                double v1 = 0.0, v2 = 0.0;

                if (!n1_is_zero) {
                    double t, val;
                    sscanf(data_map[n1][i].c_str(), "%lf,%lf", &t, &val);
                    v1 = val;
                }
                if (!n2_is_zero) {
                    double t, val;
                    sscanf(data_map[n2][i].c_str(), "%lf,%lf", &t, &val);
                    v2 = val;
                }

                double p = (v1 - v2) * i3;
                data_file << t3 << "," << p << "\n";
            }
        }
        else if (key.size() > 3 && key[0] == 'V' && key[1] == '(') {
// بررسی وجود پسوند (amp) یا (phase) در کلید ورودی
            size_t amp_pos = key.find("(amp)");
            size_t phase_pos = key.find("(phase)");

            if (amp_pos != string::npos) {
// اگر پسوند (amp) وجود دارد، فقط دامنه را نمایش می‌دهیم
                string base_key = key.substr(0, amp_pos) + "(amplitude)";
                auto it = data_map.find(base_key);
                if (it != data_map.end()) {
                    data_file << base_key << '\n';
                    for (const string &data_line : it->second) {
                        data_file << data_line << '\n';
                    }
                } else {
                    cerr << "Warning: Key '" << base_key << "' not found in log.txt" << endl;
                }
            }
            else if (phase_pos != string::npos) {
// اگر پسوند (phase) وجود دارد، فقط فاز را نمایش می‌دهیم
                string base_key = key.substr(0, phase_pos) + "(phase)";
                auto it = data_map.find(base_key);
                if (it != data_map.end()) {
                    data_file << base_key << '\n';
                    for (const string &data_line : it->second) {
                        data_file << data_line << '\n';
                    }
                } else {
                    cerr << "Warning: Key '" << base_key << "' not found in log.txt" << endl;
                }
            }
            else {
// اگر پسوند خاصی ندارد، هر دو حالت amplitude و phase را بررسی می‌کنیم
                string base_key = key;
                string amp_key = base_key + "(amplitude)";
                string phase_key = base_key + "(phase)";

                bool has_amplitude = data_map.find(amp_key) != data_map.end();
                bool has_phase = data_map.find(phase_key) != data_map.end();

                if (has_amplitude || has_phase) {
                    if (has_amplitude) {
                        data_file << amp_key << '\n';
                        for (const string &data_line : data_map[amp_key]) {
                            data_file << data_line << '\n';
                        }
                    }
                    if (has_phase) {
                        data_file << phase_key << '\n';
                        for (const string &data_line : data_map[phase_key]) {
                            data_file << data_line << '\n';
                        }
                    }
                } else {
// اگر فرمت قدیمی وجود داشت (بدون amplitude/phase)
                    auto it = data_map.find(key);
                    if (it != data_map.end()) {
                        data_file << key << '\n';
                        for (const string &data_line : it->second) {
                            data_file << data_line << '\n';
                        }
                    } else {
                        cerr << "Warning: Key '" << key << "' not found in log.txt" << endl;
                    }
                }
            }
        }
        else if (key == "V(N00)") {
            data_file << key << '\n';
            if (!data_map.empty()) {
                auto &any_data = data_map.begin()->second;
                for (const string &line : any_data) {
                    double t, dummy;
                    sscanf(line.c_str(), "%lf,%lf", &t, &dummy);
                    data_file << t << ",0.0\n";
                }
            }
        }
        else if (data_map.count(key)) {
// پردازش کلیدهای ساده که مستقیماً در فایل لاگ وجود دارند
            data_file << key << '\n';
            for (const std::string &data_line : data_map[key]) {
                data_file << data_line << '\n';
            }
        }
        else {
// ---> بخش پردازش عبارت ریاضی (به‌روز شده) <---
            try {
                std::vector<std::string> tokens = tokenize_expression(key);
                std::vector<std::string> signal_names;
                bool all_data_available = true;

                for (const std::string& token : tokens) {
                    if (token[0] == 'V' || token[0] == 'I') {
                        if (std::find(signal_names.begin(), signal_names.end(), token) == signal_names.end()) {
                            signal_names.push_back(token);
                        }
                    }
                }

                for (const std::string& name : signal_names) {
                    std::string mapped_key = map_signal_key(name); // نگاشت نام به کلید data_map
                    if (data_map.find(mapped_key) == data_map.end() && name != "V(N00)") {
                        std::cerr << "Warning: In expression '" << key << "', data for '" << name << "' (mapped to '" << mapped_key << "') not found. Skipping expression." << std::endl;
                        all_data_available = false;
                        break;
                    }
                }

                if (!all_data_available) continue;
                if (data_map.empty()) {
                    std::cerr << "Warning: Cannot process expression '" << key << "' because log data is empty." << std::endl;
                    continue;
                }

                std::vector<std::string> postfix_tokens = infix_to_postfix(tokens);
                size_t num_points = data_map.begin()->second.size();
                data_file << key << '\n';

                for (size_t i = 0; i < num_points; ++i) {
                    std::map<std::string, double> current_values;
                    double t, val;

                    for (const std::string& name : signal_names) {
                        if (name == "V(N00)") {
                            current_values[name] = 0.0;
                        } else {
                            std::string mapped_key = map_signal_key(name); // نگاشت نام برای خواندن داده
                            sscanf(data_map.at(mapped_key)[i].c_str(), "%lf,%lf", &t, &val);
                            current_values[name] = val; // ذخیره با نام اصلی برای ارزیابی
                        }
                    }

                    double current_time, dummy_val;
                    sscanf(data_map.begin()->second[i].c_str(), "%lf,%lf", &current_time, &dummy_val);

                    double result = evaluate_postfix(postfix_tokens, current_values);
                    data_file << current_time << "," << result << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not evaluate expression '" << key << "'. It may be an unknown key or an invalid expression. Error: " << e.what() << std::endl;
            }
        }
    }

    data_file.close();
    cout << "Data extracted successfully to data.txt" << endl;
}

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("CircuNet", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WindowW, WindowH, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Surface* icon = SDL_LoadBMP("assets/icon.bmp");
    if (!icon) {
        std::cerr << "Error in icon: " << SDL_GetError() << std::endl;
    } else {
        SDL_SetWindowIcon(win, icon);
        SDL_FreeSurface(icon); // بعد از تنظیم آزاد میشه
    }
    shared_ptr<TTF_Font> font(TTF_OpenFont("assets/Tahoma.ttf", 14), TTF_CloseFont);
    if (!font) { SDL_Log("Error loading font: %s", TTF_GetError()); return 1; }
    SDL_StartTextInput();
    bool running = true;
    SDL_Event e;
/////------------------------------------------------------------
    messageBox errorBox=messageBox(100, 100, 400, 200, font, font, "there is an error", "error");
    vector<Button> buttonsToolbar = createToolbar();
    vector<Button> buttonsLibrary = createLibrary();
    PopupMenu saveMenu= createSaveMenu();
    PopupMenu componentMenu= createComponentMenu();
    PopupMenu fileMenu= createFileMenu();
    PopupMenu networkMenu= createNetWorkMenu();
    PopupMenu serverMenu= createServerMenu();
    PopupMenu clientMenu= createClientMenu();
    PopupMenu AnalyzeMenu=createAnalyzeMenu();
    ElementDialog elementDialog(font);
    AnalyzeDialog analyzeDialog(font);

    Wire wire;

    LabelDialog labelDialog(200, 200, font);
    vector<shared_ptr<LabelNet>> labels;
    // ایجاد نودها با فاصله مناسب
    for (int y = stepY/2; y < WindowH; y += stepY) {
        for (int x = stepX/2; x < WindowW; x += stepX) {
            wire.allNodes.push_back(make_shared<Node>(x, y));
        }
    }

    // لیست المان‌های مداری
    vector<shared_ptr<Element>> elements;

    vector<shared_ptr<GNDSymbol>>gndSymbols;



    // وضعیت قرار دادن المان جدید
    bool placingElement = false;
    string elementTypeToPlace;
    string elementNameToPlace;
    string elementValueToPlace;
    string elementModelToplace;
    string elementGainToplace;
    string elementCtrlElementToPlace;
    string elementCtrlNodeNToPlace;
    string elementCtrlNodePToPlace;
    string elementNameSToplace;
    string phase;
    string bfr;
    string nameNP;
    string nameNN;
    string frequency;
    string vOffset;
    string vAmplitude;
    string Vinitial;
    string Von;
    string Tdelay;
    string Trise;
    string Tfall;
    string Ton;
    string Tperiod;
    string Ncycles="0";
    string tPulse;
    string area="";

    //تغیر های تحلیل
    string analyzeType,
            transientStart,
            transientStop,
            transientStep,
            dcSource,
            dcStart,
            dcEnd,
            dcStep,
            acSweepType,
            acStartFreq,
            acStopFreq,
            acPoints,
            phaseBaseFreq,
            phaseStart,
            phaseStop,
            phasePoints;

    vector<Probe> probes;
    string probeExpressions; // رشته ذخیره دستورات پروب
    Probe::ProbeType currentProbeType;


    //حالت پاک کن
    bool isDeleteModel= false;
    //حالت تقارن
    bool isMirrorModel= false;
    //حالت پروب
    bool placingProbe = false;

    // تابع برای غیرفعال کردن همه دکمه‌های کتابخانه
    auto deactivateOtherModes = [&](Button* currentBtn = nullptr) {
        wire.isActive = false;
        isDeleteModel = false;
        placingElement = false;
        placingProbe = false;
        isMirrorModel = false;
//        componentMenu.hide();
//        AnalyzeMenu.hide();

        // ریست رنگ دکمه‌ها (به جز دکمه جاری)
        for (auto& btn : buttonsLibrary) {
            if (currentBtn && &btn == currentBtn) continue;
            btn.bgColor = {100, 149, 237, 255}; // رنگ پیش‌فرض
        }
    };
    /// مربوط به منو سمت راست
    // Set component button click handler
    buttonsLibrary[0].onClick = [&]() {
        if (buttonsLibrary[0].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // همه را غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[0]);
            componentMenu.setPosition(buttonsLibrary[0].rect.x-130, buttonsLibrary[0].rect.y);
            componentMenu.toggle();
            buttonsLibrary[0].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[0].setShortcut(SDLK_q);
    componentMenu.items[0].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("Resistor");
        //کد مربوط به مقاومت
    };
    componentMenu.items[0].setShortcut(SDLK_r);
    componentMenu.items[1].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("Capacitor");
        //کد مربوط به خازن
    };
    componentMenu.items[1].setShortcut(SDLK_c);
    componentMenu.items[2].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("Inductor");
        //کد مربوط به سلف
    };
    componentMenu.items[2].setShortcut(SDLK_l);
    componentMenu.items[3].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("Diode");
        //کد مربوط به دیود
    };
    componentMenu.items[3].setShortcut(SDLK_d);
    componentMenu.items[4].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("VoltageSource");
        //کد مربوط به منبع ولتاژ
    };
    componentMenu.items[4].setShortcut(SDLK_v);
    componentMenu.items[5].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("CurrentSource");
        //کد مربوط به منبع جریان
    };
    componentMenu.items[5].setShortcut(SDLK_i);
    componentMenu.items[6].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("CCCS");
        //کد مربوط به CCCS
    };
    componentMenu.items[7].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("CCVS");
        //کد مربوط به CCVS
    };
    componentMenu.items[8].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("VCCS");
        //کد مربوط به VCCS
    };
    componentMenu.items[9].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("VCVS");
        //کد مربوط به VCVS
    };
    componentMenu.items[10].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("DeltaVoltageSource");
        //کد مربوط به منبع ولتاژ دلتا
    };
    componentMenu.items[11].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("DeltaCurrentSource");
        //کد مربوط به منبع جریان دلتا
    };
    componentMenu.items[12].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("SineVoltageSource");
        //کد مربوط به منبع ولتاژ سینوسی
    };
    componentMenu.items[13].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("SineCurrentSource");
        //کد مربوط به منبع جریان سینوسی
    };
    componentMenu.items[14].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("PulseVoltageSource");
        //کد مربوط به منبع ولتاژ پالس
    };
    componentMenu.items[15].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("PulseCurrentSource");
        //کد مربوط به منبع جریان پالس
    };
    componentMenu.items[16].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("AcVoltageSource");
        //کد مربوط به منبع ولتاژ Ac
    };
    componentMenu.items[17].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("PhaseVoltageSource");
        //کد مربوط به منبع ولتاژ Phase
    };
    componentMenu.items[18].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[0].bgColor = {237, 149, 100, 255};
        elementDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        elementDialog.show("PWLVoltageSource");
        //کد مربوط به منبع ولتاژ PWL
    };
    // Set wire button click handler
    buttonsLibrary[1].onClick = [&]() {
        if (buttonsLibrary[1].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        }
        else {
            deactivateOtherModes(&buttonsLibrary[1]);
            wire.toggleWireMode();
            buttonsLibrary[1].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[1].setShortcut(SDLK_w);
    // Set delete button click handler
    buttonsLibrary[2].onClick = [&]() {
        if (buttonsLibrary[2].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[2]);
            isDeleteModel = true;
            buttonsLibrary[2].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[2].setShortcut(SDLK_DELETE);
    // Set analyze button click handler
    buttonsLibrary[3].onClick = [&]() {
        if (buttonsLibrary[3].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // همه را غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[3]);
            AnalyzeMenu.setPosition(buttonsLibrary[3].rect.x-90, buttonsLibrary[3].rect.y);
            AnalyzeMenu.toggle();
            buttonsLibrary[3].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    AnalyzeMenu.items[0].onClick=[&]() {
        deactivateOtherModes();
        analyzeType="OP";
        try{
            analyzeOp(errorBox, wire.componentId, elements,labels);
        }
        catch (const exception &e){
            errorBox.setMessage(e.what());
            errorBox.show();
        }
        //کد مربوط به OP
    };
    AnalyzeMenu.items[1].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[3].bgColor = {237, 149, 100, 255};
        analyzeDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        analyzeDialog.show("Transient");
        //کد مربوط به Tran
    };
    AnalyzeMenu.items[2].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[3].bgColor = {237, 149, 100, 255};
        analyzeDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        analyzeDialog.show("DC Sweep");
        //کد مربوط به DC
    };
    AnalyzeMenu.items[3].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[3].bgColor = {237, 149, 100, 255};
        analyzeDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        analyzeDialog.show("AC Sweep");
        //کد مربوط به AC
    };
    AnalyzeMenu.items[4].onClick=[&]() {
        deactivateOtherModes();
        buttonsLibrary[3].bgColor = {237, 149, 100, 255};
        analyzeDialog.setPosition(1366/2 - elementDialog.rect.w/2, 768/2 - elementDialog.rect.h/2);
        analyzeDialog.show("Phase Sweep");
        //کد مربوط به منبع PHASE
    };
    // Set GND button click handler
    buttonsLibrary[4].onClick = [&]() {
        deactivateOtherModes(&buttonsLibrary[4]);
        if (!GNDSymbol::placingInstance) {
            int x, y;
            SDL_GetMouseState(&x, &y);
            gndSymbols.push_back(make_shared<GNDSymbol>(x, y));
        }
        buttonsLibrary[4].bgColor = {237, 149, 100, 255}; // رنگ فعال
    };
    // Set LabelNet button click handler
    buttonsLibrary[5].onClick = [&]() {
        deactivateOtherModes(&buttonsLibrary[5]);
        labelDialog.show();
        buttonsLibrary[5].bgColor = {237, 149, 100, 255}; // رنگ فعال
    };
    buttonsLibrary[6].onClick = [&]() {
        if (buttonsLibrary[6].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[6]);
            placingProbe = true;
            currentProbeType = Probe::VOLTAGE;
            buttonsLibrary[6].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[7].onClick = [&]() {
        if (buttonsLibrary[7].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[7]);
            placingProbe = true;
            currentProbeType = Probe::CURRENT;
            buttonsLibrary[7].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[8].onClick = [&]() {
        if (buttonsLibrary[8].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        } else {
            deactivateOtherModes(&buttonsLibrary[8]);
            placingProbe = true;
            currentProbeType = Probe::POWER;
            buttonsLibrary[8].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
    };
    buttonsLibrary[9].onClick = [&]() {
        deactivateOtherModes(&buttonsLibrary[9]);
        extract_data(probeExpressions,elements);
        DataVisualizer visualizer("data.txt");
        visualizer.run();
        buttonsLibrary[9].bgColor = {237, 149, 100, 255}; // رنگ فعال
    };
    buttonsLibrary[10].onClick = [&]() {
        if (buttonsLibrary[10].bgColor.r == 237) { // اگر از قبل فعال است
            deactivateOtherModes(); // غیرفعال کن
        }
        else {
            deactivateOtherModes(&buttonsLibrary[10]);
            isMirrorModel=!isMirrorModel;
            buttonsLibrary[10].bgColor = {237, 149, 100, 255}; // رنگ فعال
        }
        //آینه
    };
    // Set element dialog OK handler
    elementDialog.onOK = [&](const std::vector<std::string>& values) {
        placingElement = true;
        elementTypeToPlace = elementDialog.currentElementType;
        elementNameToPlace = values[0];

        if(elementTypeToPlace == "Resistor" || elementTypeToPlace == "Capacitor" ||
           elementTypeToPlace == "Inductor" || elementTypeToPlace == "VoltageSource" ||
           elementTypeToPlace == "CurrentSource") {
            elementValueToPlace = values[1];
        }
        else if(elementTypeToPlace == "AcVoltageSource") {
            elementValueToPlace = values[1];
            phase = values[2];
        }
        else if(elementTypeToPlace == "PhaseVoltageSource") {
            elementValueToPlace = values[1];
            bfr = values[2];
        }
        else if(elementTypeToPlace == "PWLVoltageSource") {
            elementValueToPlace = "data/PWL.txt";
        }
        else if(elementTypeToPlace == "Diode") {
            elementModelToplace = values[1];
        }
        else if(elementTypeToPlace == "CCCS" || elementTypeToPlace == "CCVS") {
            elementGainToplace = values[1];
            elementCtrlElementToPlace = values[2];
        }
        else if(elementTypeToPlace == "VCCS" || elementTypeToPlace == "VCVS") {
//            for (int i = 0; i < 4; ++i) {
//                cout<<values[i]<<" ";
//            }
            elementGainToplace = values[1];
            elementCtrlNodePToPlace = values[2];
            elementCtrlNodeNToPlace=values[3]; // pos and neg nodes
        }
        else if(elementTypeToPlace == "DeltaVoltageSource" || elementTypeToPlace == "DeltaCurrentSource") {
            tPulse = values[1];
            area = values[2];
        }
        else if(elementTypeToPlace == "SineVoltageSource" || elementTypeToPlace == "SineCurrentSource") {
            frequency = values[1];
            vOffset = values[2];
            vAmplitude = values[3];
        }
        else if(elementTypeToPlace == "PulseVoltageSource" || elementTypeToPlace == "PulseCurrentSource") {
            Vinitial = values[1];
            Von = values[2];
            Tdelay = values[3];
            Trise = values[4];
            Tfall = values[5];
            Ton = values[6];
            Tperiod = values[7];
            Ncycles = values[8];
        }

        SDL_Log("%s created - Name: %s", elementTypeToPlace.c_str(), elementNameToPlace.c_str());
    };
    // Set analyze dialog OK handler
    analyzeDialog.onOK = [&](const std::vector<std::string>& values){
        analyzeType=analyzeDialog.currentAnalyzeType;
        if(analyzeDialog.currentAnalyzeType == "Transient") {
            auto unitHandler=[](string input, string type)->double {
                regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
                smatch match;
                if (regex_match(input, match, pattern)) {
                    double value = stod(match[1]);
                    string unit = match[2].str();
                    double multiplier = 1.0;
                    if (unit == "n") multiplier = 1e-9;
                    else if (unit == "u") multiplier = 1e-6;
                    else if (unit == "m") multiplier = 1e-3;
                    else if (unit == "k") multiplier = 1e3;
                    else if (unit == "M" || unit == "Meg") multiplier = 1e6;
                    else if (unit == "G") multiplier = 1e9;
                    if (value < 0) {
                        throw invalidValue(type);
                    }
                    return value * multiplier;
                } else {
                    throw invalidValue(type);
                }
                return -1.0;
            };
            transientStart = values[0];
            transientStop = values[1];
            transientStep = values[2];
            SDL_Log("Transient analysis set: Start=%s, Stop=%s, Step=%s",
                    transientStart.c_str(), transientStop.c_str(), transientStep.c_str());

            try{
                cout<<analyzeTransient(unitHandler(transientStart,"StartTime"),unitHandler(transientStop,"StopTime"),unitHandler(transientStep,"Time Step")
                        ,Wire::allNodes,elements);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeDialog.currentAnalyzeType == "DC Sweep") {
            dcSource = values[0];
            dcStart = values[1];
            dcEnd = values[2];
            dcStep = values[3];
            SDL_Log("DC Sweep set: Source=%s, Start=%s, End=%s, Step=%s",
                    dcSource.c_str(), dcStart.c_str(), dcEnd.c_str(), dcStep.c_str());
            try{
                cout<<DCSweep(wire.componentId,elements,dcSource,dcStart,dcEnd,dcStep,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeDialog.currentAnalyzeType == "AC Sweep") {
            acSweepType = values[0];
            acStartFreq = values[1];
            acStopFreq = values[2];
            acPoints = values[3];
            SDL_Log("AC Sweep set: Type=%s, Start=%s, Stop=%s, Points=%s",
                    acSweepType.c_str(), acStartFreq.c_str(), acStopFreq.c_str(), acPoints.c_str());

            try{
                cout<<analyzeAc(acSweepType,acStartFreq,acStopFreq,acPoints,wire.componentId,elements,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeDialog.currentAnalyzeType == "Phase Sweep") {
            phaseBaseFreq = values[0];
            phaseStart = values[1];
            phaseStop = values[2];
            phasePoints = values[3];
            SDL_Log("Phase Sweep set: Base=%s, Start=%s, Stop=%s, Points=%s",
                    phaseBaseFreq.c_str(), phaseStart.c_str(), phaseStop.c_str(), phasePoints.c_str());

            try{
                cout<<analyzePhase(phaseStart,phaseStop,phasePoints,wire.componentId,phaseBaseFreq,elements,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }

        // Here you would typically add code to actually perform the analysis
        // or store the parameters for later use
    };

    ///مربوط به Toolbar
    //file
    // --- Load lambda ---
    auto loadAll = [&](const std::string& filename) {

        std::ifstream is(filename, std::ios::binary);
        if (!is.is_open()) {
            std::cerr << "Failed to open file for saving: " << filename << "\n";
            return;
        }

        cereal::BinaryInputArchive archive(is);

        // بارگذاری داده‌ها
        archive(
                wire,
                labels,
                elements,
                gndSymbols,
                probes,
                probeExpressions,
                analyzeType,
                transientStart, transientStop, transientStep,
                dcSource, dcStart, dcEnd, dcStep,
                acSweepType, acStartFreq, acStopFreq, acPoints,
                phaseBaseFreq, phaseStart, phaseStop, phasePoints,
                liine::count,
                Wire::allNodes,
                Wire::Lines
        );

        size_t pos = filename.find_last_of("/\\");
        string dir;
        if (pos != string::npos)
            dir = filename.substr(0, pos); // فقط مسیر
        else
            dir = "."; // اگر مسیر نبود، پوشه جاری

        string logPath = dir + "/log";


        ifstream fin(logPath, ios::in);
        ofstream fout("data/log.txt",ios::out);

        if (!fin.is_open() || !fout.is_open()) {
            cerr << "Error opening files\n";
            return;
        }
        fout<<fin.rdbuf();

    };
    // --- Save lambda ---
    auto saveAll = [&](const std::string& filename) {
        std::ofstream os(filename, std::ios::binary);
        if (!os.is_open()) {
            std::cerr << "Failed to open file for saving: " << filename << "\n";
            return;
        }
        cereal::BinaryOutputArchive archive(os);
        // کلاس ها و vector ها
        archive(wire);
        archive(labels);
        archive(elements);
        archive(gndSymbols);
        archive(probes);
        archive(probeExpressions);

        // متغیرهای تحلیل
        archive(analyzeType,
                transientStart, transientStop, transientStep,
                dcSource, dcStart, dcEnd, dcStep,
                acSweepType, acStartFreq, acStopFreq, acPoints,
                phaseBaseFreq, phaseStart, phaseStop, phasePoints);

        // متغیرهای استاتیک
        archive(liine::count);
        archive(Wire::allNodes);
        archive(Wire::Lines);

        size_t pos = filename.find_last_of("/\\");
        string dir;
        if (pos != string::npos)
            dir = filename.substr(0, pos); // فقط مسیر
        else
            dir = "."; // اگر مسیر نبود، پوشه جاری

        string logPath = dir + "/log";


        ifstream fin("data/log.txt", ios::in);
        ofstream fout(logPath, ios::out);

        if (!fin.is_open() || !fout.is_open()) {
            cerr << "Error opening files\n";
            return;
        }
        fout<<fin.rdbuf();

    };
    buttonsToolbar[0].onClick = [&]() {
        SDL_Rect r = buttonsToolbar[0].rect;
        fileMenu.setPosition(r.x, r.y + r.h + 4);
        fileMenu.toggle();
    };

    fileMenu.items[0].onClick = [&]() {
        // بررسی آیا تغییرات ذخیره نشده وجود دارد
        bool hasChanges = !elements.empty() && (nameFile == "firstRun" || !isOnceSave);

        if (hasChanges) {
            // نمایش دیالوگ تأیید برای ذخیره تغییرات
            const SDL_MessageBoxButtonData buttons[] = {
                    { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "save" },
                    { 0, 2, "don't save" },
                    { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "cancel" },
            };

            const SDL_MessageBoxData messageboxdata = {
                    SDL_MESSAGEBOX_WARNING,
                    win,
                    "Changes are not saved",
                    "There are unsaved changes. Do you want to save?",
                    SDL_arraysize(buttons),
                    buttons,
                    nullptr
            };

            int buttonid;
            if (SDL_ShowMessageBox(&messageboxdata, &buttonid) >= 0) {
                if (buttonid == 0) return; // لغو
                if (buttonid == 1) { // ذخیره
                    if (!isOnceSave) {
                        SDL_SysWMinfo wmInfo;
                        SDL_VERSION(&wmInfo.version);
                        if (!SDL_GetWindowWMInfo(win, &wmInfo)) {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "خطا",
                                                     "Failed to get window info", win);
                            return;
                        }

                        std::string filename = ShowSaveDialog(wmInfo.info.win.window);
                        if (!filename.empty()) {
                            if (filename.find('.') == std::string::npos) {
                                filename += ".shirali";
                            }
                            nameFile = filename;
                            saveAll(filename);
                            isOnceSave = true;
                        } else {
                            return; // کاربر لغو کرد
                        }
                    } else {
                        saveAll(nameFile);
                    }
                }
            }
        }

        // حالا مدار را پاک کرده و فایل جدید باز کنیم
        liine::count = 1;
        Wire::allNodes.clear();
        for (int y = stepY/2; y < WindowH; y += stepY) {
            for (int x = stepX/2; x < WindowW; x += stepX) {
                wire.allNodes.push_back(make_shared<Node>(x, y));
            }
        }
        Wire::Lines.clear();
        LabelNet::placingInstance = nullptr;
        GNDSymbol::placingInstance = nullptr;
        labels.clear();
        elements.clear();
        gndSymbols.clear();
        placingElement = false;
        analyzeType = "";
        isDeleteModel = false;
        probes.clear();
        probeExpressions = "";
        placingProbe = false;

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(win, &wmInfo)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                     "Failed to get window info", win);
            return;
        }

        std::string filename = ShowOpenFileDialog(wmInfo.info.win.window);
        if (!filename.empty()) {
            loadAll(filename);
            nameFile = filename;
            isOnceSave = true;
        } else {
            nameFile = "firstRun";
            isOnceSave = false;
        }
    };

    fileMenu.items[1].onClick = [&]() {
        // بررسی آیا تغییرات ذخیره نشده وجود دارد
        bool hasChanges = !elements.empty() && (nameFile == "firstRun" || !isOnceSave);

        if (hasChanges) {
            // نمایش دیالوگ تأیید برای ذخیره تغییرات
            const SDL_MessageBoxButtonData buttons[] = {
                    { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "save" },
                    { 0, 2, "don't save" },
                    { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "cancel" },
            };

            const SDL_MessageBoxData messageboxdata = {
                    SDL_MESSAGEBOX_WARNING,
                    win,
                    "Changes are not saved",
                    "There are unsaved changes. Do you want to save?",
                    SDL_arraysize(buttons),
                    buttons,
                    nullptr
            };

            int buttonid;
            if (SDL_ShowMessageBox(&messageboxdata, &buttonid) >= 0) {
                if (buttonid == 0) return; // لغو
                if (buttonid == 1) { // ذخیره
                    if (!isOnceSave) {
                        SDL_SysWMinfo wmInfo;
                        SDL_VERSION(&wmInfo.version);
                        if (!SDL_GetWindowWMInfo(win, &wmInfo)) {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "خطا",
                                                     "Failed to get window info", win);
                            return;
                        }

                        std::string filename = ShowSaveDialog(wmInfo.info.win.window);
                        if (!filename.empty()) {
                            if (filename.find('.') == std::string::npos) {
                                filename += ".shirali";
                            }
                            nameFile = filename;
                            saveAll(filename);
                            isOnceSave = true;
                        } else {
                            return; // کاربر لغو کرد
                        }
                    } else {
                        saveAll(nameFile);
                    }
                }
            }
        }

        // حالا مدار جدید ایجاد کنیم
        liine::count = 1;
        Wire::allNodes.clear();
        for (int y = stepY/2; y < WindowH; y += stepY) {
            for (int x = stepX/2; x < WindowW; x += stepX) {
                wire.allNodes.push_back(make_shared<Node>(x, y));
            }
        }
        Wire::Lines.clear();
        LabelNet::placingInstance = nullptr;
        GNDSymbol::placingInstance = nullptr;
        labels.clear();
        elements.clear();
        gndSymbols.clear();
        placingElement = false;
        analyzeType = "";
        isDeleteModel = false;
        probes.clear();
        probeExpressions = "";
        placingProbe = false;
        nameFile = "firstRun";
        isOnceSave = false;
    };

    buttonsToolbar[1].onClick = [&]() {
        SDL_Rect r = buttonsToolbar[1].rect;
        saveMenu.setPosition(r.x, r.y + r.h + 4);
        saveMenu.toggle();
    };
    buttonsToolbar[1].setShortcut(SDLK_s);

    saveMenu.items[0].onClick = [&]() {
        if (!elements.empty()) {
            if (!isOnceSave) {
                SDL_SysWMinfo wmInfo;
                SDL_VERSION(&wmInfo.version);
                if (!SDL_GetWindowWMInfo(win, &wmInfo)) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                             "Failed to get window info", win);
                    return;
                }

                std::string filename = ShowSaveDialog(wmInfo.info.win.window);
                if (!filename.empty()) {
                    if (filename.find('.') == std::string::npos) {
                        filename += ".shirali";
                    }
                    nameFile = filename;
                    saveAll(filename);
                    isOnceSave = true;
                }
            } else {
                saveAll(nameFile);
            }
        }
    };

    saveMenu.items[0].setShortcut(SDLK_s, KMOD_LCTRL);

    saveMenu.items[1].onClick = [&]() {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(win, &wmInfo)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                     "Failed to get window info", win);
            return;
        }

        std::string filename = ShowSaveDialog(wmInfo.info.win.window);
        if (!filename.empty()) {
            if (filename.find('.') == std::string::npos) {
                filename += ".shirali";
            }
            nameFile = filename;
            saveAll(filename);
            isOnceSave = true;
        }
    };

    saveMenu.items[1].setShortcut(SDLK_s, KMOD_LSHIFT);
    saveMenu.items[2].onClick=[&](){

    };
    //libFolder
    buttonsToolbar[2].onClick = [&](){};

    //NetWork
    buttonsToolbar[3].onClick = [&](){
        SDL_Rect r = buttonsToolbar[3].rect;
        networkMenu.setPosition(r.x, r.y + r.h + 4);
        networkMenu.toggle();
    };
    networkMenu.items[0].onClick=[&](){
        SDL_Rect r =networkMenu.items[1].rect;
        serverMenu.setPosition(r.x, r.y + r.h + 4);
        serverMenu.toggle();

//        std::thread serverThread(startServer);
//        serverThread.detach();
    };
    serverMenu.items[0].onClick=[&](){
        shared_ptr<SinVoltageSource> x;
        for (auto &i:elements) {
            x = dynamic_pointer_cast<SinVoltageSource>(i);
            if(x){
                thread serverVThread(startServerSendVoltageSource,x);
                serverVThread.detach();
                break;
            }
        }
    };
    serverMenu.items[1].onClick=[&](){
        thread serverC([&](){
            startServerSendCircuit(wire, labels, elements, gndSymbols);
        });
        serverC.detach();
    };
    serverMenu.items[2].onClick=[&](){
        vector<string>v;
        v.insert(v.end(), {
                analyzeType,
                transientStart,
                transientStop,
                transientStep,
                acSweepType,
                acStartFreq,
                acStopFreq,
                acPoints,
                phaseBaseFreq,
                phaseStart,
                phaseStop,
                phasePoints
        });

        thread serverA(startServerSendAnalyze,v);
        serverA.join();

    };
    networkMenu.items[1].onClick=[&](){
        SDL_Rect r =networkMenu.items[1].rect;
        clientMenu.setPosition(r.x, r.y + r.h + 4);
        clientMenu.toggle();

//        std::thread clientThread(startClient);
//        clientThread.detach();

    };
    clientMenu.items[0].onClick=[&](){
        shared_ptr<SinVoltageSource> x;
        for (auto &i:elements) {
            x = dynamic_pointer_cast<SinVoltageSource>(i);
            if(x){
                thread clientVThread(startClientReceiveVoltageSource,x);
                clientVThread.detach();
                break;
            }
        }
    };
    clientMenu.items[1].onClick=[&](){
        thread t1([&](){
            startClientReceiveCircuit(wire, labels, elements, gndSymbols);
        });
        t1.detach();
    };
    clientMenu.items[2].onClick=[&](){
        thread t([&](){
            vector<string> receivedData = startClientReceiveAnalyze();
            analyzeType=receivedData[0],
            transientStart=receivedData[1],
            transientStop=receivedData[2],
            transientStep=receivedData[3],
            acSweepType=receivedData[4],
            acStartFreq=receivedData[5],
            acStopFreq=receivedData[6],
            acPoints=receivedData[7],
            phaseBaseFreq=receivedData[8],
            phaseStart=receivedData[9],
            phaseStop=receivedData[10],
            phasePoints=receivedData[11];
        });
        t.join();
        if(analyzeType == "Transient") {
            auto unitHandler=[](string input, string type)->double {
                regex pattern(R"(^\s*([+-]?[\d]*\.?[\d]+(?:[eE][+-]?[\d]+)?)\s*(Meg|[numkMG])?\s*$)");
                smatch match;
                if (regex_match(input, match, pattern)) {
                    double value = stod(match[1]);
                    string unit = match[2].str();
                    double multiplier = 1.0;
                    if (unit == "n") multiplier = 1e-9;
                    else if (unit == "u") multiplier = 1e-6;
                    else if (unit == "m") multiplier = 1e-3;
                    else if (unit == "k") multiplier = 1e3;
                    else if (unit == "M" || unit == "Meg") multiplier = 1e6;
                    else if (unit == "G") multiplier = 1e9;
                    if (value < 0) {
                        throw invalidValue(type);
                    }
                    return value * multiplier;
                } else {
                    throw invalidValue(type);
                }
                return -1.0;
            };
            SDL_Log("Transient analysis set: Start=%s, Stop=%s, Step=%s",
                    transientStart.c_str(), transientStop.c_str(), transientStep.c_str());
            try{
                cout<<analyzeTransient(unitHandler(transientStart,"StartTime"),unitHandler(transientStop,"StopTime"),unitHandler(transientStep,"Time Step")
                        ,Wire::allNodes,elements);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeType == "AC Sweep") {
            SDL_Log("AC Sweep set: Type=%s, Start=%s, Stop=%s, Points=%s",
                    acSweepType.c_str(), acStartFreq.c_str(), acStopFreq.c_str(), acPoints.c_str());

            try{
                cout<<analyzeAc(acSweepType,acStartFreq,acStopFreq,acPoints,wire.componentId,elements,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeType == "Phase Sweep") {
            SDL_Log("Phase Sweep set: Base=%s, Start=%s, Stop=%s, Points=%s",
                    phaseBaseFreq.c_str(), phaseStart.c_str(), phaseStop.c_str(), phasePoints.c_str());

            try{
                cout<<analyzePhase(acStartFreq,acStopFreq,acPoints,wire.componentId,phaseBaseFreq,elements,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeType== "OP"){
            SDL_Log("op");
            try{
                analyzeOp(errorBox, wire.componentId, elements,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
        else if(analyzeType == "DC Sweep") {
            SDL_Log("DC Sweep set: Source=%s, Start=%s, End=%s, Step=%s",
                    dcSource.c_str(), dcStart.c_str(), dcEnd.c_str(), dcStep.c_str());
            try{
                cout<<DCSweep(wire.componentId,elements,dcSource,dcStart,dcEnd,dcStep,labels);
            }
            catch (const exception &e){
                errorBox.setMessage(e.what());
                errorBox.show();
            }
        }
    };
//------------------------------------------------
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            bool eventHandled = false;

            // هندلرهای منوها و دیالوگ‌ها

            if (!eventHandled) eventHandled = serverMenu.handleEvent(e);
            if (!eventHandled) eventHandled = clientMenu.handleEvent(e);
            if (!eventHandled) eventHandled = networkMenu.handleEvent(e);
            if (!eventHandled) eventHandled = saveMenu.handleEvent(e);
            if (!eventHandled) eventHandled = fileMenu.handleEvent(e);
            if (!eventHandled) eventHandled = componentMenu.handleEvent(e);
            if (!eventHandled) eventHandled = AnalyzeMenu.handleEvent(e);
            if (!eventHandled) eventHandled = elementDialog.handleEvent(e);
            if (!eventHandled) eventHandled = analyzeDialog.handleEvent(e);
            if (!eventHandled) eventHandled = labelDialog.handleEvent(e);
            if (!eventHandled) eventHandled = errorBox.handleEvent(e);

            // هندلر کلیک روی دکمه‌ها
            if (!eventHandled){
                bool buttonClicked = false;
                for (auto& b : buttonsToolbar) {
                    b.handleEvent(e);
                    if (b.clicked) {
                        buttonClicked = true;
                        b.clicked = false;
                    }
                }
                for (auto& b : buttonsLibrary) {
                    b.handleEvent(e);
                    if (b.clicked) {
                        buttonClicked = true;
                        b.clicked = false;
                    }
                }
                for (auto& b : wire.allNodes) {
                    b->handleEvent(e);
                }


                // اگر روی دکمه‌ای کلیک شد، از پردازش بیشتر صرف‌نظر کن
                if (buttonClicked) continue;

                // اگر دیالوگ تایید شد و متن داشت، یک لیبل جدید ایجاد کنید
                if (!labelDialog.visible && !labelDialog.labelText.empty()) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);

                    shared_ptr<LabelNet> newLabel = make_shared<LabelNet>(mouseX, mouseY, labelDialog.labelText, font);
                    newLabel->isPlacing = true;
                    LabelNet::placingInstance = newLabel;
                    labels.push_back(newLabel);

                    labelDialog.labelText = "";
                }

                // هندلر لیبل در حال قرارگیری
                if (LabelNet::placingInstance) {
                    LabelNet::placingInstance->handleEvent(e);

                    // اگر قرارگیری لغو شد
                    if (!LabelNet::placingInstance->isPlacing) {
                        if (LabelNet::placingInstance == LabelNet::placingInstance->placingInstance) {
                            // این حالت وقتی است که لیبل جدیدی ایجاد نشده (یعنی با ESC لغو شده)
                            labels.pop_back(); // آخرین لیبل را حذف می‌کنیم
                            LabelNet::placingInstance = nullptr;
                        } else {
                            LabelNet::placingInstance = LabelNet::placingInstance->placingInstance;
                        }
                    }
                }

                // هندلر لیبل‌های موجود
                for (auto& label : labels) {
                    if (label != LabelNet::placingInstance) {
                        label->handleEvent(e);
                    }
                }

                // در حلقه رویدادها:
                if (GNDSymbol::placingInstance) {
                    GNDSymbol::placingInstance->handleEvent(e);

                    // فقط اگر کلیک شده و isPlacing false شده
                    if (!GNDSymbol::placingInstance->isPlacing) {
                        GNDSymbol::placingInstance = nullptr; // اینجا null کنید
                    }
                }

                // در بخش رسم:
                if (GNDSymbol::placingInstance && GNDSymbol::placingInstance->isPlacing) {
                    GNDSymbol::placingInstance->draw(ren);
                }

                // هندلر کلیک ماوس برای وایرکشی
                if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = e.button.x;
                    int mouseY = e.button.y;

                    if (wire.isActive) {
                        wire.handleMouseClick(mouseX, mouseY);
                    }
                    else if (isDeleteModel) {
                        int mouseX = e.button.x;
                        int mouseY = e.button.y;
                        snapToGrid(mouseX, mouseY);// مطمئن شوید روی شبکه قرار می‌گیرد
                        SDL_Point p={mouseX,mouseY};

                        bool selectObject=false;
                        for (int i = 0; i < elements.size() ; ++i) {
                            selectObject=SDL_PointInRect(&p, &elements[i]->rect);
                            if(selectObject) {
                                elements.erase(elements.begin() + i);
                                break;
                            }
                        }
                        if(!selectObject){
                            auto min=[](int a1,int a2)->int{
                                if (a1<a2)
                                    return a1;
                                else
                                    return a2;
                            };
                            for (int i = 0; i <wire.Lines.size() ; ++i){
                                SDL_Rect Rect;
                                int x1=wire.Lines[i]->startNode->x;
                                int x2=wire.Lines[i]->endNode->x;
                                int y1=wire.Lines[i]->startNode->y;
                                int y2=wire.Lines[i]->endNode->y;
                                if(x1==x2){
                                    Rect.x=x1;
                                    Rect.y= min(y1,y2);
                                    Rect.w=2;
                                    Rect.h=std::abs(y1-y2);

                                }
                                else if(y1==y2){
                                    Rect.y=y1;
                                    Rect.x= min(x1,x2);
                                    Rect.h=2;
                                    Rect.w=std::abs(x1-x2);
                                }
                                selectObject=SDL_PointInRect(&p,&Rect);
                                if(selectObject) {
                                    wire.Lines.erase(wire.Lines.begin()+i);
                                    break;
                                }
                            }
                        }
                        if(!selectObject){
                            for (int i = 0; i < labels.size() ; ++i) {
                                selectObject=SDL_PointInRect(&p, &labels[i]->rect);
                                if(selectObject) {
                                    labels.erase(labels.begin() + i);
                                    break;
                                }
                            }
                        }
                        if(!selectObject){
                            for (int i = 0; i < gndSymbols.size() ; ++i) {
                                selectObject= gndSymbols[i]->containsPoint(p.x,p.y);
                                if(selectObject) {
                                    gndSymbols.erase(gndSymbols.begin() + i);
                                    break;
                                }
                            }
                        }

                    }
                    else if (placingElement)  {
                        int mouseX = e.button.x;
                        int mouseY = e.button.y;
                        //snapToGrid(mouseX, mouseY); // مطمئن شوید روی شبکه قرار می‌گیرد
                        try{
                            addElement(errorBox,elements,mouseX,mouseY,elementTypeToPlace,elementNameToPlace,elementValueToPlace,elementModelToplace,elementGainToplace,elementCtrlElementToPlace,elementCtrlNodePToPlace,elementCtrlNodeNToPlace,elementModelToplace,nameNP,nameNN,phase,bfr);
                            addElementTime(errorBox,elements,mouseX,mouseY,elementTypeToPlace,elementNameToPlace,frequency,vOffset,vAmplitude,Vinitial,Von,Tdelay,Trise,Tfall,Ton,Tperiod,Ncycles,tPulse,area);
                        }
                        catch(const exception&e){
                            cout<<e.what();
                            errorBox.setMessage(e.what());
                            errorBox.setTitle("ERROR!");
                            errorBox.show();
                        }
                        placingElement = false;
                        deactivateOtherModes(); // بعد از قرار دادن المان، همه دکمه‌ها غیرفعال شوند
                    }
                    else if (placingProbe) {
                        snapToGrid(mouseX, mouseY);

                        SDL_Point mousePoint = {mouseX, mouseY}; // ایجاد نقطه ماوس

                        // بررسی آیا روی المان کلیک شده است
                        bool clickedOnElement = false;
                        for (const auto& element : elements) {
                            if (SDL_PointInRect(&mousePoint, &element->rect)) {
                                if (currentProbeType == Probe::VOLTAGE) {
                                    // برای ولتاژ، نودهای المان را بررسی می‌کنیم
                                    probes.emplace_back(Probe::VOLTAGE, element->getNodeP()->name);
                                    probes.emplace_back(Probe::VOLTAGE, element->getNodeN()->name);
                                } else {
                                    // برای جریان و توان، نام المان را ذخیره می‌کنیم
                                    probes.emplace_back(currentProbeType, element->getName());
                                }
                                clickedOnElement = true;
                                break;
                            }
                        }

                        // اگر روی المان کلیک نشده، احتمالا روی نود کلیک شده
                        if (!clickedOnElement) {
                            auto node = Wire::findNode(mouseX, mouseY);
                            if (node && currentProbeType == Probe::VOLTAGE) {
                                probes.emplace_back(Probe::VOLTAGE, node->name);
                            }
                        }

                        // به‌روزرسانی رشته پروب‌ها
                        probeExpressions.clear();
                        for (const auto& probe : probes) {
                            if (!probeExpressions.empty()) {
                                probeExpressions += "\n";
                            }
                            probeExpressions += probe.expression;
                        }
                        placingProbe = false;
                        deactivateOtherModes();
                    }
                    else if (isMirrorModel){
                        bool T=false;
                        SDL_Point p={mouseX,mouseY};
                        for(auto &i:elements){
                            T= SDL_PointInRect(&p,&i->rect);
                            if(T){
                                i->Mirror();
                                break;
                            }
                        }
                    }
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_z &&
                    (e.key.keysym.mod & KMOD_CTRL)) {
                    if (!probes.empty()) {
                        probes.pop_back(); // حذف آخرین پروب
                        probeExpressions.clear();
                        for (const auto& probe : probes) {
                            if (!probeExpressions.empty()) {
                                probeExpressions += "\n";
                            }
                            probeExpressions += probe.expression;
                        }// به‌روزرسانی لیست
                    }
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_p &&
                    e.key.keysym.mod & KMOD_CTRL){
                    cout<<"Enter Output"<<endl;
                    string in="";
                    while (true){
                        string s;
                        cin>>s;
                        if (s=="end")
                            break;
                        in+=s+"\n";
                    }
                    probeExpressions = in;
                }
                // به‌روزرسانی موقعیت ماوس برای خط موقت
                if (wire.isActive) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    wire.updateMousePosition(mouseX, mouseY);
                }
            }
        }

        // رسم
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        for (auto& node : Wire::allNodes) node->draw(ren, font.get());

        // رسم خطوط (هم کامل شده و هم موقت)
        wire.draw(ren);
        wire.deleteline();
        wire.newCircuit();
        for (auto &gnd : gndSymbols) {
            if (!gnd->isPlacing) { // فقط نمادهای قرار داده شده را پردازش کنیم
                gnd->RenameNode();
            }
        }
        for (auto &i:labels) {
            i->RenameNodes();
        }


        // رسم المان‌ها، دکمه‌ها و سایر عناصر
        for (auto& element : elements) element->draw(ren, font.get());
        // رسم تمام GNDها
        for (auto& gnd : gndSymbols) {
            gnd->draw(ren);
        }
        if (GNDSymbol::placingInstance) {
            GNDSymbol::placingInstance->draw(ren);
        }

        for (auto& b : buttonsToolbar) b.draw(ren);
        for (auto& b : buttonsLibrary) b.draw(ren);
        // رسم لیبل‌ها
        for (auto& label : labels) {
            label->draw(ren);
        }
        if(placingElement) {
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            SDL_Rect previewRect{mouseX - 30, mouseY - 15, 60, 30};
            SDL_SetRenderDrawColor(ren, 200, 200, 255, 150); // آبی روشن نیمه شفاف
            SDL_RenderFillRect(ren, &previewRect);
            SDL_SetRenderDrawColor(ren, 0, 0, 200, 150);
            SDL_RenderDrawRect(ren, &previewRect);
        }
        for (const auto& probe : probes) {
            string probeText;
            SDL_Color probeColor;

            switch (probe.type) {
                case Probe::VOLTAGE:
                    probeText = "V(" + probe.target + ")";
                    probeColor = {0, 0, 255, 255}; // آبی
                    break;
                case Probe::CURRENT:
                    probeText = "I(" + probe.target + ")";
                    probeColor = {255, 0, 0, 255}; // قرمز
                    break;
                case Probe::POWER:
                    probeText = "P(" + probe.target + ")";
                    probeColor = {0, 255, 0, 255}; // سبز
                    break;
            }

            if (font) {
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font.get(), probeText.c_str(), probeColor);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
                    SDL_Rect dst{10, 50 + 25 * (&probe - &probes[0]), surf->w, surf->h};
                    SDL_RenderCopy(ren, tex, nullptr, &dst);
                    SDL_FreeSurface(surf);
                    SDL_DestroyTexture(tex);
                }
            }
        }
        saveMenu.draw(ren);
        fileMenu.draw(ren);
        componentMenu.draw(ren);
        AnalyzeMenu.draw(ren);
        elementDialog.draw(ren);
        analyzeDialog.draw(ren);
        labelDialog.draw(ren);
        errorBox.draw(ren);

        networkMenu.draw(ren);
        clientMenu.draw(ren);
        serverMenu.draw(ren);

        SDL_RenderPresent(ren);
    }
    wire.clear();
    SDL_StopTextInput();
    //TTF_CloseFont(font);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}