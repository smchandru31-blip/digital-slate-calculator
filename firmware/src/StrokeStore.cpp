#include "StrokeStore.hpp"

StrokeStore::StrokeStore()
    : _total_points(0)
    , _stroke_count(0)
    , _active_stroke(false) {
}

void StrokeStore::clear() {
    _total_points = 0;
    _stroke_count = 0;
    _active_stroke = false;
}

bool StrokeStore::startStroke() {
    if (_stroke_count >= MAX_STROKES || _total_points >= MAX_POINTS) {
        return false;
    }
    _strokes[_stroke_count].start_index = (uint16_t)_total_points;
    _strokes[_stroke_count].point_count = 0;
    _active_stroke = true;
    return true;
}

bool StrokeStore::addPoint(int16_t x, int16_t y, uint32_t t) {
    if (!_active_stroke || _total_points >= MAX_POINTS) {
        return false;
    }
    _points[_total_points].x = x;
    _points[_total_points].y = y;
    _points[_total_points].t = t;
    _total_points++;
    _strokes[_stroke_count].point_count++;
    return true;
}

void StrokeStore::endStroke() {
    if (_active_stroke) {
        // If stroke has at least 1 point, commit it
        if (_strokes[_stroke_count].point_count > 0) {
            _stroke_count++;
        }
        _active_stroke = false;
    }
}

bool StrokeStore::undoLastStroke() {
    if (_active_stroke) {
        endStroke();
    }
    if (_stroke_count == 0) {
        return false;
    }
    _stroke_count--;
    _total_points = _strokes[_stroke_count].start_index;
    return true;
}

size_t StrokeStore::getStrokeCount() const {
    return _stroke_count;
}

size_t StrokeStore::getTotalPointCount() const {
    return _total_points;
}

bool StrokeStore::isStrokeActive() const {
    return _active_stroke;
}

const StrokeSegment* StrokeStore::getStroke(size_t index) const {
    if (index >= _stroke_count) {
        return nullptr;
    }
    return &_strokes[index];
}

const StrokePoint* StrokeStore::getPoint(size_t index) const {
    if (index >= _total_points) {
        return nullptr;
    }
    return &_points[index];
}

void StrokeStore::getBounds(int16_t& min_x, int16_t& min_y, int16_t& max_x, int16_t& max_y) const {
    if (_total_points == 0) {
        min_x = min_y = max_x = max_y = 0;
        return;
    }
    min_x = max_x = _points[0].x;
    min_y = max_y = _points[0].y;
    for (size_t i = 1; i < _total_points; ++i) {
        if (_points[i].x < min_x) min_x = _points[i].x;
        if (_points[i].x > max_x) max_x = _points[i].x;
        if (_points[i].y < min_y) min_y = _points[i].y;
        if (_points[i].y > max_y) max_y = _points[i].y;
    }
}

void StrokeStore::getStrokeBounds(size_t stroke_index, int16_t& min_x, int16_t& min_y, int16_t& max_x, int16_t& max_y) const {
    if (stroke_index >= _stroke_count || _strokes[stroke_index].point_count == 0) {
        min_x = min_y = max_x = max_y = 0;
        return;
    }
    const StrokeSegment& seg = _strokes[stroke_index];
    min_x = max_x = _points[seg.start_index].x;
    min_y = max_y = _points[seg.start_index].y;
    for (uint16_t i = 1; i < seg.point_count; ++i) {
        const StrokePoint& pt = _points[seg.start_index + i];
        if (pt.x < min_x) min_x = pt.x;
        if (pt.x > max_x) max_x = pt.x;
        if (pt.y < min_y) min_y = pt.y;
        if (pt.y > max_y) max_y = pt.y;
    }
}
