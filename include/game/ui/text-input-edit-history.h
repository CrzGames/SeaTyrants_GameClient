#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct TextInputEditSnapshot {
    std::string text{};
    std::size_t cursorIndex = 0U;
    std::size_t selectionAnchorIndex = 0U;
};

class TextInputEditHistory {
public:
    explicit TextInputEditHistory(std::size_t maxEntries = 64U)
        : maxEntries(maxEntries)
    {
    }

    void clear(void)
    {
        this->undoStack.clear();
    }

    void rememberState(
        const std::string& text,
        std::size_t cursorIndex,
        std::size_t selectionAnchorIndex)
    {
        TextInputEditSnapshot snapshot{};
        snapshot.text = text;
        snapshot.cursorIndex = cursorIndex;
        snapshot.selectionAnchorIndex = selectionAnchorIndex;

        if (!this->undoStack.empty())
        {
            const TextInputEditSnapshot& previous = this->undoStack.back();
            if (previous.text == snapshot.text &&
                previous.cursorIndex == snapshot.cursorIndex &&
                previous.selectionAnchorIndex == snapshot.selectionAnchorIndex)
            {
                return;
            }
        }

        if (this->undoStack.size() >= this->maxEntries)
        {
            this->undoStack.erase(this->undoStack.begin());
        }
        this->undoStack.push_back(std::move(snapshot));
    }

    bool undo(
        std::string* text,
        std::size_t* cursorIndex,
        std::size_t* selectionAnchorIndex)
    {
        if (text == nullptr ||
            cursorIndex == nullptr ||
            selectionAnchorIndex == nullptr ||
            this->undoStack.empty())
        {
            return false;
        }

        const TextInputEditSnapshot snapshot = this->undoStack.back();
        this->undoStack.pop_back();

        *text = snapshot.text;
        *cursorIndex = (std::min)(snapshot.cursorIndex, text->size());
        *selectionAnchorIndex = (std::min)(snapshot.selectionAnchorIndex, text->size());
        return true;
    }

private:
    std::vector<TextInputEditSnapshot> undoStack{};
    std::size_t maxEntries = 64U;
};
