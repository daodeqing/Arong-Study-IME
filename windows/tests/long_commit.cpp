#include "../../engine/contracts/long_commit.h"
int main()
{
    using namespace FanyImeLongCommit;
    for (std::size_t size : {1u, 199u, 200u, 800u, 4096u})
    {
        const Text text(size, static_cast<FanyImeWireChar>('x'));
        const auto frames = Encode(text, 17);
        Assembler assembler;
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            const auto result = assembler.Append(frames[i]);
            if (result != (i + 1 == frames.size() ? Assembler::Result::Complete : Assembler::Result::Pending))
                return 1;
        }
        if (assembler.text() != text)
            return 2;
    }
    auto frames = Encode(Text(600, 'a'), 23);
    Assembler out_of_order;
    if (out_of_order.Append(frames[1]) != Assembler::Result::Invalid)
        return 3;
    Assembler wrong_request;
    wrong_request.Append(frames[0]);
    ++frames[1].request_id;
    if (wrong_request.Append(frames[1]) != Assembler::Result::Invalid)
        return 4;
    Text surrogate(195, 'a');
    surrogate += static_cast<FanyImeWireChar>(0xD83D);
    surrogate += static_cast<FanyImeWireChar>(0xDE00);
    surrogate += Text(50, 'b');
    Assembler unicode;
    for (const auto &frame : Encode(surrogate, 29))
        if (unicode.Append(frame) == Assembler::Result::Invalid)
            return 5;
    if (unicode.text() != surrogate || !Encode(Text(4097, 'x'), 3).empty() || !Encode(Text(10, 'x'), 0).empty())
        return 6;
    return 0;
}
