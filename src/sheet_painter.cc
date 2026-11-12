#include "vellum/sheet_painter.hh"
#include "vellum/printing.hh"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <wx/dc.h>
namespace vellum {
namespace {
wxRect pixel_rectangle(page_rectangle area, const print_options &options, const wxRect &paper) {
    const double xscale = paper.width / options.paper.width;
    const double yscale = paper.height / options.paper.height;
    const int left = paper.x + static_cast<int>(std::floor(area.x * xscale));
    const int top = paper.y + static_cast<int>(std::floor(area.y * yscale));
    const int right = paper.x + static_cast<int>(std::ceil((area.x + area.width) * xscale));
    const int bottom = paper.y + static_cast<int>(std::ceil((area.y + area.height) * yscale));
    return {left, top, std::max(1, right - left), std::max(1, bottom - top)};
}
} // namespace
sheet_painter::sheet_painter(pdf_document &document) : document(document) {}
void sheet_painter::paint_tile(drawing_context &context, const print_placement &placement,
                               page_rectangle tile, page_extent source) {
    const auto &box = placement.destination;
    const double u0 = std::clamp((tile.x - box.x) / box.width, 0.0, 1.0);
    const double v0 = std::clamp((tile.y - box.y) / box.height, 0.0, 1.0);
    const double u1 = std::clamp((tile.x + tile.width - box.x) / box.width, 0.0, 1.0);
    const double v1 = std::clamp((tile.y + tile.height - box.y) / box.height, 0.0, 1.0);
    const page_rectangle region =
        placement.clockwise ? page_rectangle{v0 * source.width, (1 - u1) * source.height,
                                             (v1 - v0) * source.width, (u1 - u0) * source.height}
                            : page_rectangle{u0 * source.width, v0 * source.height,
                                             (u1 - u0) * source.width, (v1 - v0) * source.height};
    const double scale =
        context.dpi / 72.0 * box.width / (placement.clockwise ? source.height : source.width);
    const auto rendered = this->document.render_print_region(
        placement.page - 1, region, static_cast<float>(scale), context.dpi);
    wxImage image(rendered.width, rendered.height);
    if (!image.IsOk()) {
        throw std::runtime_error("cannot allocate print tile");
    }
    std::copy(rendered.rgb.begin(), rendered.rgb.end(), image.GetData());
    if (placement.clockwise) {
        image = image.Rotate90(true);
    }
    if (context.options.grayscale) {
        image = image.ConvertToGreyscale();
    }
    if (!image.IsOk()) {
        throw std::runtime_error("cannot transform print tile");
    }
    const auto area = rendered.raster_area;
    const page_rectangle covered =
        placement.clockwise
            ? page_rectangle{box.x + (1 - (area.y + area.height) / source.height) * box.width,
                             box.y + area.x / source.width * box.height,
                             area.height / source.height * box.width,
                             area.width / source.width * box.height}
            : page_rectangle{box.x + area.x / source.width * box.width,
                             box.y + area.y / source.height * box.height,
                             area.width / source.width * box.width,
                             area.height / source.height * box.height};
    const auto destination = pixel_rectangle(covered, context.options, context.paper);
    const wxDCClipper clipping(context.dc, pixel_rectangle(tile, context.options, context.paper));
    wxBitmap bitmap(image);
    if (!bitmap.IsOk()) {
        throw std::runtime_error("cannot allocate print bitmap");
    }
    wxMemoryDC source_dc(bitmap);
    if (!context.dc.StretchBlit(destination.x, destination.y, destination.width, destination.height,
                                &source_dc, 0, 0, image.GetWidth(), image.GetHeight(), wxCOPY,
                                false)) {
        throw std::runtime_error("print device cannot draw bitmap");
    }
}
void sheet_painter::paint_placement(drawing_context &context, const print_placement &placement) {
    if (placement.page == 0) {
        return;
    }
    const auto &box = placement.destination;
    const auto &clip = placement.clip;
    const double left = std::max(box.x, clip.x), top = std::max(box.y, clip.y);
    const double right = std::min(box.x + box.width, clip.x + clip.width);
    const double bottom = std::min(box.y + box.height, clip.y + clip.height);
    if (right <= left || bottom <= top) {
        return;
    }
    const auto source = this->document.dimensions(placement.page - 1);
    // Rasterize visible regions at physical output DPI, not an enlarged whole-page bitmap.
    const double step = 2048 * 72.0 / context.dpi;
    const int columns = static_cast<int>(std::ceil((right - left) / step));
    const int rows = static_cast<int>(std::ceil((bottom - top) / step));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const double x = left + column * step, y = top + row * step;
            this->paint_tile(context, placement,
                             {x, y, std::min(step, right - x), std::min(step, bottom - y)}, source);
        }
    }
}
void sheet_painter::paint(wxDC &dc, const print_job &job, std::size_t side_index,
                          const wxRect &paper, int dpi, bool guides) {
    if (side_index >= job.sides.size() || paper.width <= 0 || paper.height <= 0 || dpi < 36 ||
        dpi > 600) {
        throw std::invalid_argument("invalid print render request");
    }
    require_print_permission(this->document, dpi);
    drawing_context context{dc, job.options, paper, dpi};
    const auto &side = job.sides[side_index];
    const wxDCClipper paper_clip(dc, paper);
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(paper);
    for (const auto &placement : side.placements) {
        this->paint_placement(context, placement);
        if (job.options.borders) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(*wxBLACK_PEN);
            dc.DrawRectangle(pixel_rectangle(placement.clip, job.options, paper));
        }
    }
    const auto printable = pixel_rectangle(job.options.printable, job.options, paper);
    if (guides) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(120, 120, 120), 1, wxPENSTYLE_DOT));
        dc.DrawRectangle(printable);
    }
    if (job.options.cut_marks && job.options.layout == print_layout::poster) {
        dc.SetPen(*wxBLACK_PEN);
        for (const auto &placement : side.placements) {
            const auto edge = pixel_rectangle(placement.clip, job.options, paper);
            for (const int x : {edge.GetLeft(), edge.GetRight()}) {
                for (const int y : {edge.GetTop(), edge.GetBottom()}) {
                    dc.DrawLine(x - 4, y, x + 4, y);
                    dc.DrawLine(x, y - 4, x, y + 4);
                }
            }
        }
    }
    if (job.options.tile_labels && job.options.layout == print_layout::poster) {
        dc.SetTextForeground(*wxBLACK);
        dc.SetFont(wxFontInfo(8));
        dc.DrawText(wxString::Format(wxString::FromUTF8("Page %d — tile %d,%d"),
                                     side.placements.front().page, side.tile_column + 1,
                                     side.tile_row + 1),
                    printable.x, printable.GetBottom() - 12);
    }
}
} // namespace vellum
