#include "gameLayer.h"
#include "opengl2Dlib.h"
#include <SFML/Audio.hpp>
#include "mapRenderer.h"
#include "mapData.h"
#include "math.h"
#include <fstream>
#include "imgui.h"
#include <string>
#include <iostream>
#include <filesystem>

#pragma region Macros

#define BACKGROUND_R 33
#define BACKGROUND_G 38
#define BACKGROUND_B 63

#define BACKGROUNDF_R ((float)33 / (float)0xff)
#define BACKGROUNDF_G ((float)38 / (float)0xff)
#define BACKGROUNDF_B ((float)63 / (float)0xff)

#pragma endregion

#pragma region Variables
extern gl2d::internal::ShaderProgram maskShader;
extern GLint maskSamplerUniform;

gl2d::Renderer2D renderer2d;
//sf::Music music;
MapRenderer mapRenderer;
MapData mapData;

gl2d::Texture sprites;

gl2d::FrameBuffer backGroundFBO;
gl2d::Texture backgroundTexture;

std::vector<signData> signs;
std::vector<doorData> doors;
std::vector<torchData> torches;

std::vector<unsigned short> coppyedBlocks;
int coppiedSizeX = 0;
int coppiedSizeY = 0;
int coppiedBeginX = 0;
int coppiedBeginY = 0;

std::vector<signData> coppiedSigns;
std::vector<doorData> coppiedDoors;
std::vector<torchData> coppiedTorches;

unsigned short currentBlock = Block::blueNoSolid1;

int mapWidth, mapHeight;
char mapName[256] = {};
unsigned short* map = nullptr;

bool collidable = true;
bool nonCollidable = true;
bool showBoxes = true;
bool showInvisibleBoxes = true;
bool showDangers = false;
bool simulateLights = false;
bool simulateUnlitLights = false;
bool highlightCheckPoints = false;
bool showLightAreas = false;

int editorOption = 0; //0 place blocks, 1 edit items

char signStr[256] = {};
glm::ivec2 itemPos;

glm::ivec2 itemPosEditorBegin;
glm::ivec2 itemPosEditorEnd;

int levelId = -2;
float torchLight = 5;
float torchXBox= 0;
float torchYBox= 0;
#pragma endregion

float getTorchLight(int x, int y)
{
	auto iter = std::find_if(torches.begin(), torches.end(),
		[x, y](torchData &d)->bool {return (d.pos.x == x && d.pos.y == y); });

	if (iter != torches.end())
	{
		return iter->light;
	}
	else
	{
		return 5;
	}
}

void placeBlock(unsigned short type, int x, int y, MapData &mapData)
{
	if (type == Block::flagUp)
	{
		for (int x = 0; x < mapData.w; x++)
			for (int y = 0; y < mapData.h; y++)
			{
				if (mapData.get(x, y).type == Block::flagUp)
				{
					mapData.get(x, y).type = Block::flagDown;
				}
			}
	}

	mapData.get(x, y).type = type;

}

void placeBlockSafe(unsigned short type, int x, int y, MapData &mapData)
{
	if(x >= 0 && x < mapData.w && y >=0 && y <mapData.h)
	{
		placeBlock(type, x, y, mapData);
	}
}

void eraseBlockSafe(int x, int y, MapData &mapData)
{
	placeBlockSafe(Block::none, x, y, mapData);
}

void eraseBlock(int x, int y, MapData &mapData)
{
	placeBlock(Block::none, x, y, mapData);
}

bool initGame()
{
	renderer2d.create();
	ShaderProgram sp{ "resources//blocks.vert","resources//blocks.frag" };
	sprites.loadFromFile("resources//sprites.png");
	backGroundFBO.create(40 * BLOCK_SIZE, 40 * BLOCK_SIZE);

	mapRenderer.init(sp);
	mapRenderer.sprites = sprites;
	mapRenderer.upTexture.loadFromFile("resources//top.png");
	mapRenderer.downTexture.loadFromFile("resources//bottom.png");
	mapRenderer.leftTexture.loadFromFile("resources//left.png");
	mapRenderer.rightTexture.loadFromFile("resources//right.png");

	mapData.create(40, 40, map);

	mapWidth = mapData.w;
	mapHeight = mapData.h;

	glClearColor(BACKGROUNDF_R, BACKGROUNDF_G, BACKGROUNDF_B, 1.f);

	return true;
}

bool gameLogic(float deltaTime)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	int w = getWindowSizeX();
	int h = getWindowSizeY();

	glViewport(0, 0, w, h);
	renderer2d.updateWindowMetrics(w, h);

	constexpr float speed = 600.f;

#pragma region Camera Movement
	if (platform::isKeyHeld('Q'))
	{
		renderer2d.currentCamera.zoom -= deltaTime * 3;
	}
	if (platform::isKeyHeld('E'))
	{
		renderer2d.currentCamera.zoom += deltaTime * 3;
	}
	if (platform::isKeyHeld('D'))
	{
		renderer2d.currentCamera.position.x += deltaTime * speed;
	}
	if (platform::isKeyHeld('A'))
	{
		renderer2d.currentCamera.position.x -= deltaTime * speed;
	}
	if (platform::isKeyHeld('W'))
	{
		renderer2d.currentCamera.position.y -= deltaTime * speed;
	}
	if (platform::isKeyHeld('S'))
	{
		renderer2d.currentCamera.position.y += deltaTime * speed;
	}
#pragma endregion

	mapData.clearColorData();

#pragma region Adding Blocks Into the World


	glm::vec2 mousePos;
	mousePos.x = (platform::getRelMousePosition().x + renderer2d.currentCamera.position.x);
	mousePos.y = (platform::getRelMousePosition().y + renderer2d.currentCamera.position.y);

	mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
				glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);

	if (editorOption == 1)
	{

		if (platform::isLMouseButtonPressed())
		{
			
			if (mousePos.x / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
				|| mousePos.y / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
			{

			}
			else
			{
				auto block = mapData.get(mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE).type;

#pragma region Edit Signs
				if (isSign(block))
				{
					auto it = std::find_if(signs.begin(), signs.end(),
						[x = (int)mousePos.x / BLOCK_SIZE, y = (int)mousePos.y / BLOCK_SIZE](signData& d)
					{ return d.pos.x == x && d.pos.y == y; });
					if (it != signs.end())
					{
						int j = 0;
						for (auto i : it->text)
						{
							signStr[j++] = i;
						}
						signStr[j] = NULL;

						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
					else
					{
						signStr[0] = 0;
						std::string s = "";
						glm::ivec2 pos = { mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE };
						signData d(pos, s);
						signs.emplace_back(d);

						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
				}
#pragma endregion

#pragma region Edit Door
				if (isDoor(block))
				{
					auto it = std::find_if(doors.begin(), doors.end(),
						[x = (int)mousePos.x / BLOCK_SIZE, y = (int)mousePos.y / BLOCK_SIZE](doorData& d)
					{ return d.pos.x == x && d.pos.y == y; });

					if (it != doors.end())
					{
						levelId = it->levelId;
						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
					else
					{
						glm::ivec2 pos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
						doorData d(pos, -2);
						doors.emplace_back(d);
						levelId = -2;
						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
				}
#pragma endregion

#pragma region Edit Torch
				if (isLitTorch(block) ||
					unLitTorch(block))
				{
					auto it = std::find_if(torches.begin(), torches.end(),
						[x = (int)mousePos.x / BLOCK_SIZE, y = (int)mousePos.y / BLOCK_SIZE](torchData& d)
					{ return d.pos.x == x && d.pos.y == y; });

					if (it != torches.end())
					{
						torchLight = it->light;
						torchXBox = it->xBox;
						torchYBox = it->yBox;
						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
					else
					{
						glm::ivec2 pos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
						torchData d(pos, 5);
						torches.emplace_back(d);

						torchLight = 5;
						torchXBox = 0;
						torchYBox = 0;
						itemPos = { (int)mousePos.x / BLOCK_SIZE, (int)mousePos.y / BLOCK_SIZE };
					}
				}
#pragma endregion
			}
		}
	}

	if (editorOption == 2)
	{
		
		if(platform::isLMouseButtonPressed())
		{
			itemPosEditorBegin = { mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE };		
			itemPosEditorEnd = { mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE };
		}

		if(platform::isLMouseHeld())
		{
			itemPosEditorEnd = { mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE };
		}else
		{
			int beginx = std::min(itemPosEditorBegin.x, itemPosEditorEnd.x);
			int beginy = std::min(itemPosEditorBegin.y, itemPosEditorEnd.y);;
			int endx = std::max(itemPosEditorBegin.x, itemPosEditorEnd.x);;
			int endy = std::max(itemPosEditorBegin.y, itemPosEditorEnd.y);;

			itemPosEditorBegin = { beginx, beginy };
			itemPosEditorEnd = { endx, endy };

			if(itemPosEditorBegin.x < 0)
			{
				itemPosEditorBegin.x = 0;
			}
			if (itemPosEditorBegin.y < 0)
			{
				itemPosEditorBegin.y = 0;
			}
			if (itemPosEditorEnd.x >= mapData.w)
			{
				itemPosEditorBegin.x = mapData.w-1;
			}
			if (itemPosEditorEnd.y >= mapData.h)
			{
				itemPosEditorBegin.y = mapData.h - 1;
			}

		}

		{
			int beginx = std::min(itemPosEditorBegin.x, itemPosEditorEnd.x);
			int beginy = std::min(itemPosEditorBegin.y, itemPosEditorEnd.y);;
			int endx = std::max(itemPosEditorBegin.x, itemPosEditorEnd.x);;
			int endy = std::max(itemPosEditorBegin.y, itemPosEditorEnd.y);;

			glm::ivec2 b = { beginx, beginy };
			glm::ivec2 e = { endx, endy };

			renderer2d.renderRectangle({ b.x * BLOCK_SIZE, b.y * BLOCK_SIZE,
			(e.x - b.x + 1) * BLOCK_SIZE,
			(e.y - b.y + 1) * BLOCK_SIZE },
			{ 1,1,0.2,0.2 });
		}
		

	}

	if (editorOption == 0)
	{

	#pragma region Eye Dropper Tool
		if (platform::isKeyHeld(VK_CONTROL))
		{
			if (platform::isLMouseButtonPressed() || platform::isRMouseButtonPressed())
			{
				glm::vec2 mousePos;
				mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
				mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

				mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
					glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);
				if (mousePos.x / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
					|| mousePos.y / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
				{

				}
				else
				{
					if (mapData.get(mousePos.x / BLOCK_SIZE, (mousePos.y) / BLOCK_SIZE).type != Block::none)
						currentBlock = mapData.get(mousePos.x / BLOCK_SIZE, (mousePos.y) / BLOCK_SIZE).type;
				}
			}
		}
	#pragma endregion

		else
		{
		#pragma region Place Blocks
			if (platform::isLMouseHeld())
			{
				glm::vec2 mousePos;
				mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
				mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

				mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
					glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);

				if ((mousePos.x) / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
					|| (mousePos.y) / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
				{

				}
				else
				{
					placeBlock(currentBlock, mousePos.x / BLOCK_SIZE, mousePos.y / BLOCK_SIZE,
						mapData);

				}

			}
		#pragma endregion

		#pragma region Eraser
			else if (platform::isRMouseHeld())
			{
				glm::vec2 mousePos;
				mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
				mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

				mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
					glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);
				if (mousePos.x / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
					|| mousePos.y / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
				{

				}
				else
				{
					eraseBlock((mousePos.x) / BLOCK_SIZE, (mousePos.y) / BLOCK_SIZE, mapData);
				}

			}
		#pragma endregion

		#pragma region Render the current block
			else
			{
				glm::vec2 mousePos;
				mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
				mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

				mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
					glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);
				if (mousePos.x / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
					|| mousePos.y / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
				{

				}
				else
				{
					gl2d::TextureAtlas spriteAtlas(BLOCK_COUNT, 4);

					renderer2d.renderRectangle({ (int)(mousePos.x / BLOCK_SIZE) * BLOCK_SIZE , (int)(mousePos.y / BLOCK_SIZE) * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE },
						{}, 0, sprites, { spriteAtlas.get(currentBlock - 1, 0).x, spriteAtlas.get(currentBlock - 1,0).y,
						 spriteAtlas.get(currentBlock - 1, 0).z, spriteAtlas.get(currentBlock - 1,0).w });
				}
			}
		#pragma endregion

		}
	}

#pragma endregion

#pragma region Render the blocks

	if (simulateLights || simulateUnlitLights || showLightAreas)
	{
		for (int x = 0; x < mapData.w; x++)
			for (int y = 0; y < mapData.h; y++)
			{
				if (isLitTorch(mapData.get(x, y).type) && simulateLights)
				{
					float light = getTorchLight(x, y);
					simuleteLightSpot({ x * BLOCK_SIZE + BLOCK_SIZE / 2,y * BLOCK_SIZE + BLOCK_SIZE / 2 },
						light, mapData);
				}

				if (unLitTorch(mapData.get(x, y).type) && simulateUnlitLights)
				{
					float light = getTorchLight(x, y);
					simuleteLightSpot({ x * BLOCK_SIZE + BLOCK_SIZE / 2,y * BLOCK_SIZE + BLOCK_SIZE / 2 },
						light, mapData);
				}

				if(showLightAreas && (isLitTorch(mapData.get(x, y).type) || unLitTorch(mapData.get(x, y).type)) )
				{
					float light = getTorchLight(x, y);
					renderer2d.renderRectangle(
						{ x * BLOCK_SIZE + BLOCK_SIZE/2 - ((light * BLOCK_SIZE)), 
						y * BLOCK_SIZE + BLOCK_SIZE / 2 - ((light * BLOCK_SIZE)),
					(light * 2.f) * BLOCK_SIZE,
					(light * 2.f) * BLOCK_SIZE },
					{ 1,0.2,0.3,0.08 });
				}

			}

		if(!simulateLights && !simulateUnlitLights)
		{
			for (int x = 0; x < mapData.w; x++)
				for (int y = 0; y < mapData.h; y++)
				{
					mapData.get(x, y).mainColor = { 1,1,1,1 };
				}
		}
	}
	else
	{
		for (int x = 0; x < mapData.w; x++)
			for (int y = 0; y < mapData.h; y++)
			{
				mapData.get(x, y).mainColor = { 1,1,1,1 };
			}
	}

	for (int x = 0; x < mapData.w; x++)
	{
		for (int y = 0; y < mapData.h; y++)
		{
			auto light = mapData.get(x, y).mainColor.w;
			mapData.get(x, y).mainColor.w = 0;
			if (collidable) {
				if (isCollidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor.w = 1;
				}
			}
			else
			{
				if (isCollidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor.w = 0.2;
				}

			}

			if (nonCollidable)
			{
				if (!isCollidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor.w = 1;
				}
			}
			else
			{
				if (!isCollidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor.w = 0.2;
				}
			}

			//mapData.get(x, y).mainColor.w *= light;
		}
	}

	mapRenderer.drawFromMapData(renderer2d, mapData);
#pragma endregion

#pragma region Render Map Margins
	renderer2d.renderRectangle({ 0,0, mapData.w * BLOCK_SIZE, -10 }, Colors_Turqoise);
	renderer2d.renderRectangle({ 0,0,-10, mapData.h * BLOCK_SIZE }, Colors_Turqoise);
	renderer2d.renderRectangle({ 0,mapData.h * BLOCK_SIZE, mapData.w * BLOCK_SIZE, 10 }, Colors_Turqoise);
	renderer2d.renderRectangle({ mapData.w * BLOCK_SIZE,0,10,mapData.h * BLOCK_SIZE }, Colors_Turqoise);
#pragma endregion

#pragma region ImGui Check Boxes
	for (int y = 0; y < mapData.h; y++)
	{
		for (int x = 0; x < mapData.w; x++)
		{
			if (isCollidable(mapData.get(x, y).type))
			{
				if(showBoxes)
				{
					if(mapData.get(x, y).type !=Block::bareer)
					renderer2d.renderRectangle({ x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 0.5,0.1,0.5,0.2 });
				}

				if (showInvisibleBoxes)
				{
					if(mapData.get(x, y).type == Block::bareer)
					renderer2d.renderRectangle({ x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 0.5,0.1,0.5,0.2 });
				}

			}
			else if (
				(mapData.get(x, y).type == Block::water3 
					|| mapData.get(x, y).type == Block::lavaKill
					)
				
				&& showDangers)
			{
				renderer2d.renderRectangle({ x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 1,0.2,0.2,0.9 });
			}

			if (mapData.get(x, y).type == Block::flagDown && highlightCheckPoints)
			{
				renderer2d.renderRectangle({ x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 0,1,0.6,0.7 });
			}

			if (mapData.get(x, y).type == Block::flagUp && highlightCheckPoints)
			{
				renderer2d.renderRectangle({ x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 0,1,0.2,0.9 });
			}

			if (isSign(mapData.get(x, y).type) && (editorOption == 1))
			{
				// show signs
			}

			
		}

	}

	if (editorOption == 1)
	{
		renderer2d.renderRectangle({ itemPos.x * BLOCK_SIZE, itemPos.y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 1,1,0.2,0.4 });
		
		if(unLitTorch(mapData.get(itemPos.x, itemPos.y).type))
		{
			auto it = std::find_if(torches.begin(), torches.end(),
						[x = itemPos.x, y = itemPos.y](torchData &d)
			{ return d.pos.x == x && d.pos.y == y; });

			if (it != torches.end())
			{
				float w = it->xBox, h = it->yBox;

				if(w && h)
				{
					renderer2d.renderRectangle({ (itemPos.x-w/2) * BLOCK_SIZE + BLOCK_SIZE/2,
						(itemPos.y-h/2) * BLOCK_SIZE + BLOCK_SIZE/2,
						w * BLOCK_SIZE, h *BLOCK_SIZE }, { 0.8,0.7,0.3,0.1 });
				}

			}

		}
	
	}
#pragma endregion

	renderer2d.flush();

	return true;
}

void closeGame()
{
	//music.stop();
}


void imguiFunc(float deltaTime)
{
	ImGui::Begin("Map settings");

#pragma region Open Map
	static char name[256] = {};
	ImGui::InputText("OutputFile Name", name, sizeof(name));
	if (ImGui::Button("Open Map"))
	{
		renderer2d.currentCamera.setDefault();

		char aux[256];
		strcpy(aux, "resources//");
		strcat(aux, name);
		strcat(aux, ".level");
		std::ifstream inputFile(aux);

		inputFile >> mapWidth >> mapHeight;

		unsigned short blocks[500 * 500];
		int it = 0;
		std::string current;

		for (int i = 0; i <= mapHeight; i++)
		{
			std::getline(inputFile, current);
			for (auto i = 0; i < current.length(); i++)
			{
				std::string number;
				while (current[i] != ',')
				{
					number += current[i];
					i++;
				}
				blocks[it++] = static_cast<unsigned short>(std::stoi(number));
			}
		}
		blocks[it] = NULL;

		doors.clear();
		signs.clear();
		torches.clear();

		while (std::getline(inputFile, current))
		{
			if (current[0] == 0)
			{
				continue;
			}

			if (current[3] == 's' && current[4] == 'i')
			{
				int x, y;
				std::string str;
				sscanf(current.c_str(), "md.signDataVector.emplace_back( glm::ivec2{%d, %d}", &x, &y);

				int i;
				for (i = 0; i < current.length(); i++)
				{
					if (current[i] == '"')
					{
						i++;
						break;
					}
				}
				while (current[i] != '"')
				{
					str += current[i++];
				}

				signData d({ x, y }, str);
				signs.emplace_back(d);
			}
			else if (current[3] == 'e')
			{
				int x, y, id;
				sscanf(current.c_str(), "md.exitDataVector.emplace_back(glm::ivec2{%d, %d}, %d);", &x, &y, &id);
				doorData d({ x, y }, id);
				doors.emplace_back(d);
			}
			else if (current[3] == 't')
			{
				int x, y;
				float xBox = 0, yBox = 0;
				float light;
				sscanf(current.c_str(), "md.torchDataVector.emplace_back(glm::ivec2{%d, %d}, %f, %f, %f);",
					&x, &y, &light, &xBox, &yBox);

				torchData d({ x, y }, light, xBox, yBox);
				torches.emplace_back(d);
			}
		}


		inputFile.close();

		mapData.cleanup();
		mapData.create(mapWidth, mapHeight, blocks);
	}
	ImGui::SameLine();
	if (ImGui::Button("Open Template"))
	{
		renderer2d.currentCamera.setDefault();

		char aux[256];
		strcpy(aux, "resources//");
		strcat(aux, name);
		strcat(aux, ".template");
		std::ifstream inputFile(aux);

		inputFile >> mapWidth >> mapHeight;

		unsigned short blocks[500 * 500];
		int it = 0;
		std::string current;

		for (int i = 0; i <= mapHeight; i++)
		{
			std::getline(inputFile, current);
			for (auto i = 0; i < current.length(); i++)
			{
				std::string number;
				while (current[i] != ',')
				{
					number += current[i];
					i++;
				}
				blocks[it++] = static_cast<unsigned short>(std::stoi(number));
			}
		}
		blocks[it] = NULL;

		inputFile.close();

		torches.clear();
		doors.clear();
		signs.clear();

		mapData.cleanup();
		mapData.create(mapWidth, mapHeight, blocks);
	}
	ImGui::NewLine();
#pragma endregion

#pragma region Empty Map
	ImGui::InputInt("New Map Width", &mapWidth);
	ImGui::InputInt("New Map Height", &mapHeight);

	if (ImGui::Button("New Map"))
	{
		MapData temp;
		temp.create(mapWidth, mapHeight, nullptr);

		for (int x = 0; x < mapWidth && x < mapData.w; x++)
			for (int y = 0; y < mapHeight && y < mapData.h; y++)
			{
				temp.get(x, y) = mapData.get(x, y);
			}

		mapData.cleanup();
		mapData = temp;
	}
	ImGui::NewLine();
#pragma endregion

#pragma region Save Map

	if (ImGui::Button("Save Map"))
	{
		char aux[256] = {};
		strcpy(aux, "resources//");
		strcat(aux, name);
		strcat(aux, ".level");

		std::ofstream outputFile(aux);
		outputFile << mapWidth << std::endl << mapHeight << std::endl;
		for (int y = 0; y < mapHeight; y++)
		{
			for (int x = 0; x < mapWidth; x++)
			{
				outputFile << (mapData.get(x, y).type) << ",";
			}
			outputFile << "\n";
		}

		outputFile << "\n\n";


		for (auto& i : signs)
		{
			if (isSign(mapData.get(i.pos.x, i.pos.y).type))
			{
				outputFile << "md.signDataVector.emplace_back( glm::ivec2{" << i.pos.x << ", " << i.pos.y << "}, \"" << i.text << "\");\n";
			}
		}

		outputFile << "\n\n";

		for (auto& i : doors)
		{
			if (isDoor(mapData.get(i.pos.x, i.pos.y).type))
			{
				outputFile << "md.exitDataVector.emplace_back(glm::ivec2{" << i.pos.x << ", " << i.pos.y << "}, " << i.levelId << ");\n";
			}
		}

		outputFile << "\n\n";

		for (auto& i : torches)
		{
			if (isLitTorch(mapData.get(i.pos.x, i.pos.y).type)||
				unLitTorch(mapData.get(i.pos.x, i.pos.y).type))
			{
				outputFile << "md.torchDataVector.emplace_back(glm::ivec2{ " << i.pos.x << ", " << i.pos.y << "}, "
					<< i.light << ", " << i.xBox << ", " << i.yBox << ");\n";
			}
		}

		outputFile.close();
	}

	ImGui::SameLine();
	if(ImGui::Button("Save Template"))
	{
		char aux[256] = {};
		strcpy(aux, "resources//");
		strcat(aux, name);
		strcat(aux, ".template");

		std::ofstream outputFile(aux);
		outputFile << mapWidth << std::endl << mapHeight << std::endl;
		for (int y = 0; y < mapHeight; y++)
		{
			for (int x = 0; x < mapWidth; x++)
			{
				outputFile << (mapData.get(x, y).type) << ",";
			}
			outputFile << "\n";
		}
	
	}

	ImGui::NewLine();
#pragma endregion


	ImGui::Checkbox("Show Collidable Blocks", &collidable);
	ImGui::Checkbox("Show Non-Collidable Blocks", &nonCollidable);
	ImGui::Checkbox("Highlight Boxes", &showBoxes);
	ImGui::Checkbox("Highlight Invisible Boxes", &showInvisibleBoxes);

	ImGui::Checkbox("Highlight Dangers", &showDangers); ImGui::SameLine();
	ImGui::Checkbox("Simulate Lights", &simulateLights); ImGui::SameLine();
	ImGui::Checkbox("Simulate Unlit Lights", &simulateUnlitLights); ImGui::SameLine();
	ImGui::Checkbox("showLightAreas", &showLightAreas);
	ImGui::Checkbox("Highlight Check points", &highlightCheckPoints);
	ImGui::NewLine();

	const char *const items[]
	{
		"Block Builder",
		"Item editor",
		"World editor",
	};

	ImGui::ListBox("Editor type", &editorOption, items, sizeof(items) / sizeof(*items));

	if (editorOption == 1)
	{
		ImGui::InputText("Sign Text", signStr, sizeof(signStr));
#pragma region Edit Sign

		//if (ImGui::Button("Save Sign"))
		{
			auto it = std::find_if(signs.begin(), signs.end(),
				[x = itemPos.x, y = itemPos.y](signData& d)
			{ return d.pos.x == x && d.pos.y == y; });

			if(it != signs.end())
			{
				it->text = std::string(signStr);
			}

		}
#pragma endregion
		ImGui::NewLine();
#pragma region Edit Door
		ImGui::InputInt("Next Level Id", &levelId);

		//if (ImGui::Button("Save Door"))
		{
			auto it = std::find_if(doors.begin(), doors.end(),
				[x = itemPos.x, y = itemPos.y](doorData& d)
			{ return d.pos.x == x && d.pos.y == y; });
			
			if(it != doors.end())
			{
				it->levelId = levelId;
			}
			
		}
#pragma endregion
		ImGui::NewLine();
#pragma region Edit Torch
		ImGui::DragFloat("Torch", &torchLight);
		ImGui::DragFloat("Torch XBox", &torchXBox);
		ImGui::DragFloat("Torch YBox", &torchYBox);

		//if (ImGui::Button("Save Torch"))
		{
			auto it = std::find_if(torches.begin(), torches.end(),
				[x = itemPos.x, y = itemPos.y](torchData& d)
			{ return d.pos.x == x && d.pos.y == y; });


			if(it != torches.end())
			{
				it->light = torchLight;
				it->xBox = torchXBox;
				it->yBox = torchYBox;
			}

		}
#pragma endregion

	}


	if (editorOption == 0)
	{

		gl2d::TextureAtlas spriteAtlas(BLOCK_COUNT, 4);
		unsigned short mCount = 1;
		ImGui::BeginChild("Block Selector");

		if (collidable && nonCollidable)
		{
			unsigned short localCount = 0;
			while (mCount < Block::lastBlock)
			{
				if (!isUnfinished(mCount))
				{
					ImGui::PushID(mCount);
					if (ImGui::ImageButton((void *)(intptr_t)sprites.id,
						{ 35,35 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
						{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
					{
						currentBlock = mCount;
					}
					ImGui::PopID();

					if (localCount % 10 != 0)
					{
						ImGui::SameLine();
					}
					localCount++;
				}

				mCount++;
			}
		}
		else
		{
			if (collidable && !nonCollidable)
			{
				unsigned short localCount = 0;
				while (mCount < Block::lastBlock)
				{
					if (isCollidable(mCount) && !isUnfinished(mCount))
					{
						ImGui::PushID(mCount);
						if (ImGui::ImageButton((void *)(intptr_t)sprites.id,
							{ 35,35 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
							{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
						{
							currentBlock = mCount;
						}
						ImGui::PopID();

						if (localCount % 10 != 0)
							ImGui::SameLine();
						localCount++;
					}
					mCount++;
				}
			}
			else if (!collidable && nonCollidable)
			{
				unsigned short localCount = 0;
				while (mCount < Block::lastBlock)
				{
					if (!isCollidable(mCount) && !isUnfinished(mCount))
					{
						ImGui::PushID(mCount);
						if (ImGui::ImageButton((void *)(intptr_t)sprites.id,
							{ 35,35 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
							{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
						{
							currentBlock = mCount;
							//llog((int)currentBlock);
						}
						ImGui::PopID();

						if (localCount % 10 != 0)
							ImGui::SameLine();
						localCount++;
					}
					mCount++;
				}
			}
		}
		ImGui::EndChild();
	
	}


	if (editorOption == 2)
	{
		ImGui::Text("Positon: %d, %d", itemPosEditorBegin.x, itemPosEditorBegin.y);

		bool copy = ImGui::Button("Copy");
		bool del = ImGui::Button("Delete");
		bool cut = ImGui::Button("Cut");
		ImGui::NewLine();
		bool paste = ImGui::Button("Paste");
		
		if(copy || cut)
		{
			coppyedBlocks.clear();

			coppiedSizeX = itemPosEditorEnd.x - itemPosEditorBegin.x + 1;
			coppiedSizeY = itemPosEditorEnd.y - itemPosEditorBegin.y + 1;
			coppiedBeginX = itemPosEditorBegin.x;
			coppiedBeginY = itemPosEditorBegin.y;

			coppiedSigns.clear();
			coppiedDoors.clear();
			coppiedTorches.clear();
			
			
			coppyedBlocks.reserve(coppiedSizeX * coppiedSizeY);

			for (int y = itemPosEditorBegin.y; y < itemPosEditorBegin.y + coppiedSizeY; y++)
			{
				for (int x = itemPosEditorBegin.x; x < itemPosEditorBegin.x + coppiedSizeX; x++)
				{
					auto b = mapData.get(x, y);
					coppyedBlocks.push_back(b.type);
				}
			}

			for (int i = 0; i < signs.size(); i++)
			{
				auto p = signs[i].pos;

				if (p.x >= coppiedBeginX 
					&& p.y >= coppiedBeginY
					&& p.x <= itemPosEditorEnd.x
					&& p.y <= itemPosEditorEnd.y
					)
				{
					auto item = signs[i];
					if(cut)
					{
						signs.erase(signs.begin() + i);
						i--;
					}

					coppiedSigns.push_back(item);
				}
			}

			for (int i = 0; i < doors.size(); i++)
			{
				auto p = doors[i].pos;

				if (p.x >= coppiedBeginX
					&& p.y >= coppiedBeginY
					&& p.x <= itemPosEditorEnd.x
					&& p.y <= itemPosEditorEnd.y
					)
				{
					auto item = doors[i];
					if(cut)
					{
						doors.erase(doors.begin() + i);
						i--;
					}

					coppiedDoors.push_back(item);
				}
			}

			for (int i = 0; i < torches.size(); i++)
			{
				auto p = torches[i].pos;

				if (p.x >= coppiedBeginX
					&& p.y >= coppiedBeginY
					&& p.x <= itemPosEditorEnd.x
					&& p.y <= itemPosEditorEnd.y
					)
				{
					auto item = torches[i];
					if (cut)
					{
						torches.erase(torches.begin() + i);
						i--;
					}

					coppiedTorches.push_back(item);
				}
			}

		}
		
		if(del || cut)
		{
			int sizeX = itemPosEditorEnd.x - itemPosEditorBegin.x + 1;
			int sizeY = itemPosEditorEnd.y - itemPosEditorBegin.y + 1;

			for (int y = itemPosEditorBegin.y; y < itemPosEditorBegin.y + sizeY; y++)
			{
				for (int x = itemPosEditorBegin.x; x < itemPosEditorBegin.x + sizeX; x++)
				{
					eraseBlockSafe(x, y, mapData);
				}
			}
		}

		if(paste && !coppyedBlocks.empty())
		{
			int i = 0;
			for (int y = itemPosEditorBegin.y; y < itemPosEditorBegin.y + coppiedSizeY; y++)
			{
				for (int x = itemPosEditorBegin.x; x < itemPosEditorBegin.x + coppiedSizeX; x++)
				{
					auto type = coppyedBlocks[i];

					eraseBlockSafe(x, y, mapData);
					placeBlockSafe(type, x, y, mapData);

					i++;

				}
			}

			int coppiedDeltaX = itemPosEditorBegin.x - coppiedBeginX;
			int coppiedDeltaY = itemPosEditorBegin.y - coppiedBeginY;

			for (auto i : coppiedSigns)
			{
				i.pos += glm::ivec2{coppiedDeltaX, coppiedDeltaY};
				signs.push_back(i);
			}

			for (auto i : coppiedDoors)
			{
				i.pos += glm::ivec2{ coppiedDeltaX, coppiedDeltaY };
				doors.push_back(i);
			}
			
			for (auto i : coppiedTorches)
			{
				i.pos += glm::ivec2{ coppiedDeltaX, coppiedDeltaY };
				torches.push_back(i);
			}

			itemPosEditorEnd = itemPosEditorBegin + glm::ivec2{ coppiedSizeX-1 ,coppiedSizeY-1 };
		}

		{
		

			std::string path = "resources/";
			for (const auto &entry : std::filesystem::directory_iterator(path))
			{
				std::string s = entry.path().string();
				auto it = s.find(".template");
				if(it != std::string::npos)
				{
					bool copy = ImGui::Button(("Copy " + std::string(s.begin(), s.begin() + it)).c_str());

					if(copy)
					{
						
						std::ifstream inputFile(s);

						int w; int h;

						inputFile >> w >> h;

						coppyedBlocks.clear();

						coppiedSizeX = w;
						coppiedSizeY = h;

						coppyedBlocks.reserve(coppiedSizeX *coppiedSizeY);


						int it = 0;
						std::string current;

						for (int i = 0; i <= h; i++)
						{
							std::getline(inputFile, current);
							for (auto i = 0; i < current.length(); i++)
							{
								std::string number;
								while (current[i] != ',')
								{
									number += current[i];
									i++;
								}
								coppyedBlocks.push_back(static_cast<unsigned short>(std::stoi(number)));
							}
						}

						itemPosEditorEnd = itemPosEditorBegin + glm::ivec2{ coppiedSizeX - 1 ,coppiedSizeY - 1 };
					}
				}

			}

		}


		if(itemPosEditorEnd == itemPosEditorBegin && coppiedSizeX && coppiedSizeY)
		{
			itemPosEditorEnd = itemPosEditorBegin + glm::ivec2{ coppiedSizeX - 1 ,coppiedSizeY - 1 };
		}

	}

	ImGui::End();
}
