/* $Id$ */

/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file track_piece.cpp Functions of the track pieces. */

#include "stdafx.h"
#include "sprite_store.h"
#include "fileio.h"
#include "track_piece.h"
#include "map.h"

TrackVoxel::TrackVoxel()
{
	std::fill_n(this->back, lengthof(this->back), nullptr);
	std::fill_n(this->front, lengthof(this->front), nullptr);
}

TrackVoxel::~TrackVoxel()
{
	/* Images are deleted by the RCD manager. */
}

/**
 * Load a track voxel.
 * @param rcd_file Data file being loaded.
 * @param length Length of the voxel (according to the file).
 * @param sprites Already loaded sprite blocks.
 * @return Loading was successful.
 */
bool TrackVoxel::Load(RcdFile *rcd_file, size_t length, const ImageMap &sprites)
{
	if (length != 4*4 + 4*4 + 3 + 1) return false;
	for (uint i = 0; i < 4; i++) {
		if (!LoadSpriteFromFile(rcd_file, sprites, &this->back[i])) return false;
	}
	for (uint i = 0; i < 4; i++) {
		if (!LoadSpriteFromFile(rcd_file, sprites, &this->front[i])) return false;
	}
	this->dx = rcd_file->GetInt8();
	this->dy = rcd_file->GetInt8();
	this->dz = rcd_file->GetInt8();
	this->flags = rcd_file->GetUInt8();
	return true;
}

TrackCurve::TrackCurve()
{
}

TrackCurve::~TrackCurve()
{
}

/**
 * \fn double TrackCurve::GetValue(int distance)
 * Get the value of the curve at the provided \a distance.
 * @param distance Distance of the car at the curve, in 1/256 pixel.
 * @return Value of this track curve variable at the given distance.
 */

/**
 * Track curve that always has the same value.
 * @param value Constant value of the curve.
 */
ConstantTrackCurve::ConstantTrackCurve(int value) : TrackCurve(), value(value)
{
}

/**
 * Partial track curve described by a cubic Bezier spline.
 * @param start Start distance of this curve in the track piece, in 1/256 pixel.
 * @param last Last distance of this curve in the track piece, in 1/256 pixel.
 * @param a Starting value of the Bezier spline.
 * @param b First control point of the Bezier spline.
 * @param c Second intermediate control point of the Bezier spline.
 * @param d Ending value of the Bezier spline.
 */
CubicBezier::CubicBezier(uint32 start, uint32 last, int a, int b, int c, int d) : start(start), last(last), a(a), b(b), c(c), d(d)
{
}

BezierTrackCurve::BezierTrackCurve()
{
}

/**
 * Load the data of a Bezier spline into a #BezierTrackCurve.
 * @param rcdfile Data file to load from. Caller must ensure there is enough data available at the stream.
 * @return The Loaded Bezier spline.
 */
static CubicBezier LoadBezier(RcdFile *rcdfile)
{
	uint32 start = rcdfile->GetUInt32();
	uint32 last = rcdfile->GetUInt32();
	int16 a = rcdfile->GetInt16();
	int16 b = rcdfile->GetInt16();
	int16 c = rcdfile->GetInt16();
	int16 d = rcdfile->GetInt16();
	return CubicBezier(start, last, a, b, c, d);
}

/**
 * Load a track curve.
 * @param rcdfile Data file being loaded.
 * @param curve [out] The loaded track curve, may be \c nullptr (which indicates a not supplied track curve).
 * @param length [inout] Length of the block that is not loaded yet.
 * @return Reading was a success (might have failed due to not enough data).
 */
static bool LoadTrackCurve(RcdFile *rcdfile, TrackCurve **curve, uint32 *length)
{
#define ENSURE_LENGTH(x) do { if (*length < (x)) { *curve = nullptr; return false; } *length -= (x); } while(false)

	ENSURE_LENGTH(1);
	uint8 type = rcdfile->GetUInt8();
	switch (type) {
		case 0: // No track curve available.
			*curve = nullptr;
			return true;

		case 1: { // Curve consisting of a fixed value.
			ENSURE_LENGTH(2);
			int value = rcdfile->GetInt16();
			*curve = new ConstantTrackCurve(value);
			return true;
		}

		case 2: {
			ENSURE_LENGTH(1);
			int count = rcdfile->GetUInt8();
			ENSURE_LENGTH(count * 16u);
			BezierTrackCurve *bezier = new BezierTrackCurve();
			bezier->curve.reserve(count);
			for (; count > 0; count--) {
				bezier->curve.push_back(LoadBezier(rcdfile));
			}
			bezier->curve.shrink_to_fit();
			*curve = bezier;
			return true;
		}

		default: // Error.
			fprintf(stderr, "Unexpected curve type %u.\n", type);
			*curve = nullptr;
			return false;
	}
#undef ENSURE_LENGTH
}

TrackPiece::TrackPiece()
{
	this->voxel_count = 0;
	this->track_voxels = nullptr;
	this->piece_length = 0;
	this->car_xpos = nullptr;
	this->car_ypos = nullptr;
	this->car_zpos = nullptr;
	this->car_pitch = nullptr;
	this->car_roll = nullptr;
	this->car_yaw = nullptr;
}

TrackPiece::~TrackPiece()
{
	delete[] this->track_voxels;
	delete this->car_xpos;
	delete this->car_ypos;
	delete this->car_zpos;
	delete this->car_pitch;
	delete this->car_roll;
	delete this->car_yaw;
}

/**
 * Load a track piece.
 * @param rcd_file Data file being loaded.
 * @param length Length of the voxel (according to the file).
 * @param sprites Already loaded sprite blocks.
 * @return Loading was successful.
 */
bool TrackPiece::Load(RcdFile *rcd_file, uint32 length, const ImageMap &sprites)
{
	if (length < 2+3+1+2+4+2) return false;
	length -= 2+3+1+2+4+2;
	this->entry_connect = rcd_file->GetUInt8();
	this->exit_connect = rcd_file->GetUInt8();
	this->exit_dx = rcd_file->GetInt8();
	this->exit_dy = rcd_file->GetInt8();
	this->exit_dz = rcd_file->GetInt8();
	this->speed = rcd_file->GetInt8();
	this->track_flags = rcd_file->GetUInt16();
	this->cost = rcd_file->GetUInt32();
	this->voxel_count = rcd_file->GetUInt16();
	if (length < 36u * this->voxel_count) return false;
	length -= 36u * this->voxel_count;
	this->track_voxels = new TrackVoxel[this->voxel_count];
	for (int i = 0; i < this->voxel_count; i++) {
		if (!this->track_voxels[i].Load(rcd_file, 36, sprites)) return false;
	}
	if (length < 4u) return false;
	length -= 4;
	this->piece_length = rcd_file->GetUInt32();

	bool ok = true;
	ok = ok && LoadTrackCurve(rcd_file, &this->car_xpos,  &length);
	ok = ok && LoadTrackCurve(rcd_file, &this->car_ypos,  &length);
	ok = ok && LoadTrackCurve(rcd_file, &this->car_zpos,  &length);
	ok = ok && LoadTrackCurve(rcd_file, &this->car_pitch, &length);
	ok = ok && LoadTrackCurve(rcd_file, &this->car_roll,  &length);
	ok = ok && LoadTrackCurve(rcd_file, &this->car_yaw,   &length);
	if (!ok || this->car_xpos == nullptr || this->car_ypos == nullptr || this->car_zpos == nullptr || this->car_roll == nullptr) return false;
	return length == 0;
}

/** Default constructor. */
PositionedTrackPiece::PositionedTrackPiece()
{
	this->piece = nullptr;
}

/**
 * Constructor taking values for all its fields.
 * @param xpos X position of the positioned track piece.
 * @param ypos Y position of the positioned track piece.
 * @param zpos Z position of the positioned track piece.
 * @param piece Track piece to use.
 */
PositionedTrackPiece::PositionedTrackPiece(uint16 xpos, uint16 ypos, uint8 zpos, ConstTrackPiecePtr piece)
{
	this->x_base = xpos;
	this->y_base = ypos;
	this->z_base = zpos;
	this->piece = piece;
}

/**
 * Verify that all voxels of this track piece are within world boundaries.
 * @return The positioned track piece is entirely within the world boundaries.
 * @pre Positioned track piece must have a piece.
 */
bool PositionedTrackPiece::IsOnWorld() const
{
	assert(this->piece != nullptr);
	if (!IsVoxelInsideWorld(this->x_base, this->y_base, this->z_base)) return false;
	if (!IsVoxelInsideWorld(this->x_base + this->piece->exit_dx, this->y_base + this->piece->exit_dy,
			this->z_base + this->piece->exit_dz)) return false;
	const TrackVoxel *tvx = this->piece->track_voxels;
	for (int i = 0; i < this->piece->voxel_count; i++) {
		if (!IsVoxelInsideWorld(this->x_base + tvx->dx, this->y_base + tvx->dy, this->z_base + tvx->dz)) return false;
		tvx++;
	}
	return true;
}

/**
 * Can this positioned track piece be placed in the world?
 * @return Positioned track piece can be placed.
 * @pre Positioned track piece must have a piece.
 */
bool PositionedTrackPiece::CanBePlaced() const
{
	if (!this->IsOnWorld()) return false;
	const TrackVoxel *tvx = this->piece->track_voxels;
	for (int i = 0; i < this->piece->voxel_count; i++) {
		/* Is the voxel above ground level? */
		if (_world.GetGroundHeight(this->x_base + tvx->dx, this->y_base + tvx->dy) > this->z_base + tvx->dz) return false;
		const Voxel *vx = _world.GetVoxel(this->x_base + tvx->dx, this->y_base + tvx->dy, this->z_base + tvx->dz);
		if (vx != nullptr && !vx->CanPlaceInstance()) return false;
	}
	return true;
}

/**
 * Can this positioned track piece function as a successor for the given exit conditions?
 * @param x Required X position.
 * @param y Required Y position.
 * @param z Required Z position.
 * @param connect Required entry connection.
 * @return This positioned track piece can be used as successor.
 */
bool PositionedTrackPiece::CanBeSuccessor(uint16 x, uint16 y, uint8 z, uint8 connect) const
{
	return this->piece != nullptr && this->x_base == x && this->y_base == y && this->z_base == z && this->piece->entry_connect == connect;
}

/**
 * Can this positioned track piece function as a successor of piece \a pred?
 * @param pred Previous positioned track piece.
 * @return This positioned track piece can be used as successor.
 */
bool PositionedTrackPiece::CanBeSuccessor(const PositionedTrackPiece &pred) const
{
	if (pred.piece == nullptr) return false;
	return this->CanBeSuccessor(pred.GetEndX(), pred.GetEndY(), pred.GetEndZ(), pred.piece->exit_connect);
}
