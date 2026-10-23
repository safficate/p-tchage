#include "AudioMemory.h"
namespace mp {
void AudioMemory::prepare(double sr){const juce::ScopedLock guard(lock);rate=sr;queue=std::make_unique<Block[]>(capacity);fifo.reset();stored.clear();first=INT64_MAX;last=-1;droppedBlocks=0;resetRequested=false;}
void AudioMemory::push(const juce::AudioBuffer<float>& b,int64_t position)noexcept{
    if(!queue)return;
    for(int offset=0;offset<b.getNumSamples();offset+=blockSize){const int count=std::min(blockSize,b.getNumSamples()-offset);const auto write=fifo.write(1);if(write.blockSize1+write.blockSize2==0){++droppedBlocks;continue;}
        auto& slot=queue[size_t(write.blockSize1?write.startIndex1:write.startIndex2)];slot.start=position+offset;slot.count=count;
        for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i){const float x=b.getSample(std::min(ch,b.getNumChannels()-1),offset+i);slot.samples[size_t(ch)][size_t(i)]=std::isfinite(x)?x:0;}
    }
}
void AudioMemory::drain(){const juce::ScopedLock guard(lock);bool changed=false;
    if(resetRequested.exchange(false)){stored.clear();first=INT64_MAX;last=-1;droppedBlocks=0;++generation;const auto discard=fifo.read(fifo.getNumReady());return;}
    const auto add=[&](const Block& b){for(int i=0;i<b.count;){const int64_t sample=b.start+i;if(sample<0){++i;continue;}const auto key=sample/blockSize;const int inside=int(sample%blockSize),n=std::min(b.count-i,blockSize-inside);
            auto at=stored.find(key);if(at==stored.end()){if((stored.size()+1)*(sizeof(Stored)+64)>size_t(maxAudioBytes)){++droppedBlocks;return;}at=stored.try_emplace(key).first;changed=true;}
            auto& slot=at->second;for(int j=0;j<n;++j){for(int ch=0;ch<2;++ch){const float x=b.samples[size_t(ch)][size_t(i+j)];if(slot.valid[size_t(inside+j)]&&slot.samples[size_t(ch)][size_t(inside+j)]!=x)changed=true;slot.samples[size_t(ch)][size_t(inside+j)]=x;}slot.valid.set(size_t(inside+j));}
            first=std::min(first,sample);last=std::max(last,sample+n-1);i+=n;
        }};
    const auto read=fifo.read(fifo.getNumReady());for(int i=0;i<read.blockSize1;++i)add(queue[size_t(read.startIndex1+i)]);for(int i=0;i<read.blockSize2;++i)add(queue[size_t(read.startIndex2+i)]);if(changed)++generation;
}
std::shared_ptr<Clip> AudioMemory::assemble(juce::String& error){const juce::ScopedLock guard(lock);
    drain();if(last<first){error="Once kanali dinletin; kayitli ses yok.";return {};}
    const int64_t length=last-first+1;if(length>rate*maxSeconds||length*8>maxAudioBytes){error="Hazirlanacak zaman araligi 600 saniye / bellek sinirini asiyor.";return {};}
    int64_t covered=0;for(const auto& [key,slot]:stored)for(int i=0;i<blockSize;++i){const int64_t sample=key*blockSize+i;if(sample>=first&&sample<=last&&slot.valid[size_t(i)])++covered;}
    if(covered!=length){error="Duyulmamis alan var: "+juce::String((length-covered)/rate,3)+" saniye. Eksik alanlari dinletin; bosluklar ses diye doldurulmadi.";return {};}
    auto clip=std::make_shared<Clip>();clip->sampleRate=rate;clip->anchorSeconds=first/rate;clip->name="DAW kanal hafizasi";clip->audio.setSize(2,int(length));clip->audio.clear();
    for(const auto& [key,slot]:stored){const int64_t start=key*blockSize,from=std::max(start,first),to=std::min(start+blockSize,last+1);if(to<=from)continue;for(int ch=0;ch<2;++ch)clip->audio.copyFrom(ch,int(from-first),slot.samples[size_t(ch)].data()+int(from-start),int(to-from));}
    return clip;
}
}
